#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "flow.h"
#include "log.h"
#include "opts.h"
#include "peer.h"
#include "receive.h"
#include "send.h"
#include "send_receive.h"
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
static opts_group file_opts;

static char log_module = 'F';

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
	LOG(file->id, "Will retry in %ds", delay / 1000);
	file->peer.event_handler = file_open_wrapper;
	wakeup_add(&file->peer, delay);
}

static void file_handle_close(struct peer *peer) {
	struct file *file = container_of(peer, struct file, peer);
	LOG(file->id, "File closed: %s", file->path);

	if (file->retry) {
		file_retry(file);
	} else {
		file_del(file);
	}
}

static void file_open(struct file *file) {
	LOG(file->id, "Opening file: %s", file->path);
	int fd = open(file->path, file->flags | O_CLOEXEC | O_NOCTTY, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		LOG(file->id, "Error opening file: %s", strerror(errno));
		file_retry(file);
		return;
	}

	file->retry = file_should_retry(fd, file);
	file->peer.event_handler = file_handle_close;
	file->attempt = 0;
	if (!flow_new_send_hello(fd, file->flow, file->passthrough, &file->peer)) {
		LOG(file->id, "Error writing greeting");
		file_retry(file);
		return;
	}
}

static void file_open_wrapper(struct peer *peer) {
	struct file *file = container_of(peer, struct file, peer);
	file_open(file);
}

static void file_new(const char *path, int flags, struct flow *flow, void *passthrough) {
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

static bool file_write_add(const char *path, struct flow *flow, void *passthrough) {
	file_write_new(path, flow, passthrough);
	return true;
}

static bool file_append_add(const char *path, struct flow *flow, void *passthrough) {
	file_append_new(path, flow, passthrough);
	return true;
}

static bool file_read(const char *arg) {
	file_read_new(arg, receive_flow, NULL);
	return true;
}

static bool file_write(const char *arg) {
	return send_add(file_write_add, send_flow, arg);
}

static bool file_write_read(const char *arg) {
	return send_add(file_write_add, send_receive_flow, arg);
}

static bool file_append(const char *arg) {
	return send_add(file_append_add, send_flow, arg);
}

static bool file_append_read(const char *arg) {
	return send_add(file_append_add, send_receive_flow, arg);
}

void file_opts_add() {
	opts_add("file-read", "PATH", file_read, file_opts);
	opts_add("file-write", "FORMAT=PATH", file_write, file_opts);
	opts_add("file-write-read", "FORMAT=PATH", file_write_read, file_opts);
	opts_add("file-append", "FORMAT=PATH", file_append, file_opts);
	opts_add("file-append-read", "FORMAT=PATH", file_append_read, file_opts);
}

void file_init() {
	opts_call(file_opts);
}

void file_cleanup() {
	struct file *iter, *next;
	list_for_each_entry_safe(iter, next, &file_head, file_list) {
		file_del(iter);
	}
}

void file_read_new(const char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_RDONLY, flow, passthrough);
}

void file_write_new(const char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC, flow, passthrough);
}

void file_append_new(const char *path, struct flow *flow, void *passthrough) {
	file_new(path, O_WRONLY | O_CREAT | O_NOFOLLOW, flow, passthrough);
}
