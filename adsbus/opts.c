#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "exec.h"
#include "file.h"
#include "incoming.h"
#include "outgoing.h"
#include "receive.h"
#include "send.h"

#include "opts.h"

static char *opts_split(char **arg, char delim) {
	char *split = strchr(*arg, delim);
	if (!split) {
		return NULL;
	}
	char *ret = strndup(*arg, split - *arg);
	*arg = split + 1;
	return ret;
}

static void opts_add_listen(char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, flow, passthrough);
		free(host);
	} else {
		incoming_new(NULL, host_port, flow, passthrough);
	}
}

static struct serializer *opts_get_serializer(char **arg) {
	char *format = opts_split(arg, '=');
	if (!format) {
		return NULL;
	}

	struct serializer *serializer = send_get_serializer(format);
	free(format);
	if (!serializer) {
		return NULL;
	}

	return serializer;
}

bool opts_add_connect_receive(char *arg) {
	char *host = opts_split(&arg, '/');
	if (!host) {
		return false;
	}

	outgoing_new(host, arg, receive_flow, NULL);
	free(host);
	return true;
}

bool opts_add_connect_send(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	char *host = opts_split(&arg, '/');
	if (!host) {
		return false;
	}

	outgoing_new(host, arg, send_flow, serializer);
	free(host);
	return true;
}

bool opts_add_listen_receive(char *arg) {
	opts_add_listen(arg, receive_flow, NULL);
	return true;
}

bool opts_add_listen_send(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	opts_add_listen(arg, send_flow, serializer);
	return true;
}

bool opts_add_file_read(char *arg) {
	file_read_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_file_write(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	file_write_new(arg, send_flow, serializer);
	return true;
}

bool opts_add_file_append(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	file_append_new(arg, send_flow, serializer);
	return true;
}

bool opts_add_exec_receive(char *arg) {
	exec_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_exec_send(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	exec_new(arg, send_flow, serializer);
	return true;
}

bool opts_add_stdin(char __attribute__((unused)) *arg) {
	int fd = fcntl(0, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	file_fd_new(fd, receive_flow, NULL);
	return true;
}

bool opts_add_stdout(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	int fd = fcntl(1, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	file_fd_new(fd, send_flow, serializer);
	return true;
}
