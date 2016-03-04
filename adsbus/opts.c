#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "exec.h"
#include "file.h"
#include "flow.h"
#include "incoming.h"
#include "outgoing.h"
#include "receive.h"
#include "send.h"
#include "send_receive.h"

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

static bool opts_add_listen(char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, flow, passthrough);
		free(host);
	} else {
		incoming_new(NULL, host_port, flow, passthrough);
	}
	return true;
}

static bool opts_add_connect(char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (!host) {
		return false;
	}

	outgoing_new(host, host_port, flow, passthrough);
	free(host);
	return true;
}

static bool opts_add_file_write_int(char *path, struct flow *flow, void *passthrough) {
	file_write_new(path, flow, passthrough);
	return true;
}

static bool opts_add_file_append_int(char *path, struct flow *flow, void *passthrough) {
	file_append_new(path, flow, passthrough);
	return true;
}

static bool opts_add_exec(char *cmd, struct flow *flow, void *passthrough) {
	exec_new(cmd, flow, passthrough);
	return true;
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

static bool opts_add_send(bool (*next)(char *, struct flow *, void *), struct flow *flow, char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}
	return next(arg, flow, serializer);
}

bool opts_add_connect_receive(char *arg) {
	return opts_add_connect(arg, receive_flow, NULL);
}

bool opts_add_connect_send(char *arg) {
	return opts_add_send(opts_add_connect, send_flow, arg);
}

bool opts_add_connect_send_receive(char *arg) {
	return opts_add_send(opts_add_connect, send_receive_flow, arg);
}

bool opts_add_listen_receive(char *arg) {
	return opts_add_listen(arg, receive_flow, NULL);
}

bool opts_add_listen_send(char *arg) {
	return opts_add_send(opts_add_listen, send_flow, arg);
}

bool opts_add_listen_send_receive(char *arg) {
	return opts_add_send(opts_add_listen, send_receive_flow, arg);
}

bool opts_add_file_read(char *arg) {
	file_read_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_file_write(char *arg) {
	return opts_add_send(opts_add_file_write_int, send_flow, arg);
}

bool opts_add_file_write_read(char *arg) {
	return opts_add_send(opts_add_file_write_int, send_receive_flow, arg);
}

bool opts_add_file_append(char *arg) {
	return opts_add_send(opts_add_file_append_int, send_flow, arg);
}

bool opts_add_file_append_read(char *arg) {
	return opts_add_send(opts_add_file_append_int, send_receive_flow, arg);
}

bool opts_add_exec_receive(char *arg) {
	exec_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_exec_send(char *arg) {
	return opts_add_send(opts_add_exec, send_flow, arg);
}

bool opts_add_exec_send_receive(char *arg) {
	return opts_add_send(opts_add_exec, send_receive_flow, arg);
}

bool opts_add_stdin(char __attribute__((unused)) *arg) {
	int fd = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, receive_flow, NULL, NULL);
}

bool opts_add_stdout(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	int fd = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, send_flow, serializer, NULL);
}
