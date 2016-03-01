#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "flow.h"
#include "receive.h"
#include "send.h"

#include "file.h"

void file_fd_new(int fd, struct flow *flow, void *passthrough) {
	flow->new(fd, passthrough, NULL);
	// TODO: log error; retry?
	flow_hello(fd, flow, passthrough);
}

void file_read_new(char *path, struct flow *flow, void *passthrough) {
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		// TODO: log error; retry?
		return;
	}
	file_fd_new(fd, flow, passthrough);
}

void file_write_new(char *path, struct flow *flow, void *passthrough) {
	int fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		// TODO: log error; retry?
		return;
	}
	file_fd_new(fd, flow, passthrough);
}

void file_append_new(char *path, struct flow *flow, void *passthrough) {
	int fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRWXU);
	if (fd == -1) {
		// TODO: log error; retry?
		return;
	}
	flow->new(fd, flow, passthrough);
}
