#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "flow.h"
#include "list.h"
#include "peer.h"
#include "receive.h"
#include "send.h"
#include "uuid.h"
#include "wakeup.h"

#include "file.h"

struct file {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *path;
	int flags;
	uint32_t attempt;
	struct flow *flow;
	void *passthrough;
	bool retry;
	struct list_head file_list;
};

static struct list_head file_head = LIST_HEAD_INIT(file_head);

static void file_open_wrapper(struct peer *);

static bool file_should_retry(int fd, struct file *file) {
	// We want to retry most files, except if we're reading from a regular file
	if (!file->flags & O_RDONLY) {
		return true;
	}

	// Questionable heuristic time. We're going to test if the kernel driver for
	// this fd supports polling. If it does, we assume that we have a character
	// device, named pipe, or something else that is likely to give us different
	// data if we read it again.
	int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	assert(epoll_fd >= 0);
	struct epoll_event ev = {
		.events = 0,
		.data = {
			.ptr = NULL,
		},
	};
	int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	assert(!close(epoll_fd));
	return (res == 0);
}

static void file_del(struct file *file) {
	flow_ref_dec(file->flow);
	list_del(&file->file_list);
	free(file->path);
	free(file);
}

static void file_retry(struct file *file) {
	uint32_t delay = wakeup_get_retry_delay_ms(file->attempt++);
	fprintf(stderr, "F %s: Will retry in %ds\n", file->id, delay / 1000);
	file->peer.event_handler = file_open_wrapper;
	wakeup_add((struct peer *) file, delay);
}

static void file_handle_close(struct peer *peer) {
	struct file *file = (struct file *) peer;
	fprintf(stderr, "F %s: File closed: %s\n", file->id, file->path);

	if (file->retry) {
		file_retry(file);
	} else {
		file_del(file);
	}
}

static void file_open(struct file *file) {
	fprintf(stderr, "F %s: Opening file: %s\n", file->id, file->path);
	int fd = open(file->path, file->flags | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		fprintf(stderr, "F %s: Error opening file: %s\n", file->id, strerror(errno));
		file_retry(file);
		return;
	}

	file->retry = file_should_retry(fd, file);
	file->peer.event_handler = file_handle_close;
	if (!flow_hello(fd, file->flow, file->passthrough)) {
		fprintf(stderr, "F %s: Error writing greeting\n", file->id);
		file_retry(file);
		return;
	}
	file->attempt = 0;
	file->flow->new(fd, file->passthrough, (struct peer *) file);
}

static void file_open_wrapper(struct peer *peer) {
	struct file *file = (struct file *) peer;
	file_open(file);
}

static void file_new(char *path, int flags, struct flow *flow, void *passthrough) {
	flow_ref_inc(flow);

	struct file *file = malloc(sizeof(*file));
	assert(file);
	file->peer.fd = -1;
	uuid_gen(file->id);
	file->path = strdup(path);
	assert(file->path);
	file->flags = flags;
	file->attempt = 0;
	file->flow = flow;
	file->passthrough = passthrough;

	list_add(&file->file_list, &file_head);

	file_open(file);
}

void file_cleanup() {
	struct file *iter, *next;
	list_for_each_entry_safe(iter, next, &file_head, file_list) {
		file_del(iter);
	}
}

void file_read_new(char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_RDONLY, flow, passthrough);
}

void file_write_new(char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC, flow, passthrough);
}

void file_append_new(char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_WRONLY | O_CREAT | O_NOFOLLOW, flow, passthrough);
}
