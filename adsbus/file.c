#include <assert.h>
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

#include "file.h"

struct file {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *path;
	int flags;
	struct flow *flow;
	void *passthrough;
	bool retry;
	struct list_head file_list;
};

static struct list_head file_head = LIST_HEAD_INIT(file_head);

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
	(*file->flow->ref_count)--;
	list_del(&file->file_list);
	free(file->path);
	free(file);
}

static void file_handle_close(struct peer *peer) {
	struct file *file = (struct file *) peer;

	file_del(file);
}

static void file_open(struct file *file) {
	int fd = open(file->path, file->flags | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		// TODO: log error; retry?
		return;
	}

	file->retry = file_should_retry(fd, file);
	file->peer.event_handler = file_handle_close;
	file->flow->new(fd, file->passthrough, (struct peer *) file);
	// TODO: log error; retry?
	flow_hello(fd, file->flow, file->passthrough);
}

static void file_new(char *path, int flags, struct flow *flow, void *passthrough) {
	(*flow->ref_count)++;

	struct file *file = malloc(sizeof(*file));
	assert(file);
	file->peer.fd = -1;
	uuid_gen(file->id);
	file->path = strdup(path);
	assert(file->path);
	file->flags = flags;
	file->flow = flow;
	file->passthrough = passthrough;

	list_add(&file->file_list, &file_head);

	file_open(file);
}

// TODO: this code probably belongs elsewhere
void file_fd_new(int fd, struct flow *flow, void *passthrough) {
	flow->new(fd, passthrough, NULL);
	// TODO: log error; retry?
	flow_hello(fd, flow, passthrough);
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
