#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "flow.h"
#include "receive.h"
#include "send.h"

#include "file.h"

static void file_open_new(char *path, struct flow *flow, void *passthrough, int flags) {
	int fd = open(path, flags | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		// TODO: log error; retry?
		return;
	}
	file_fd_new(fd, flow, passthrough);
}

void file_fd_new(int fd, struct flow *flow, void *passthrough) {
	flow->new(fd, passthrough, NULL);
	// TODO: log error; retry?
	flow_hello(fd, flow, passthrough);
}

void file_read_new(char *path, struct flow *flow, void *passthrough) {
	file_open_new(path, flow, passthrough, O_RDONLY);
}

void file_write_new(char *path, struct flow *flow, void *passthrough) {
	file_open_new(path, flow, passthrough, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC);
}

void file_append_new(char *path, struct flow *flow, void *passthrough) {
	file_open_new(path, flow, passthrough, O_WRONLY | O_CREAT | O_NOFOLLOW);
}
