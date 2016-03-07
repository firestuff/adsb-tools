#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
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

static char *opts_split(const char **arg, char delim) {
	char *split = strchr(*arg, delim);
	if (!split) {
		return NULL;
	}
	char *ret = strndup(*arg, split - *arg);
	*arg = split + 1;
	return ret;
}

static bool opts_add_listen(const char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, flow, passthrough);
		free(host);
	} else {
		incoming_new(NULL, host_port, flow, passthrough);
	}
	return true;
}

static bool opts_add_connect(const char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (!host) {
		return false;
	}

	outgoing_new(host, host_port, flow, passthrough);
	free(host);
	return true;
}

static bool opts_add_file_write_int(const char *path, struct flow *flow, void *passthrough) {
	file_write_new(path, flow, passthrough);
	return true;
}

static bool opts_add_file_append_int(const char *path, struct flow *flow, void *passthrough) {
	file_append_new(path, flow, passthrough);
	return true;
}

static bool opts_add_exec(const char *cmd, struct flow *flow, void *passthrough) {
	exec_new(cmd, flow, passthrough);
	return true;
}

static struct serializer *opts_get_serializer(const char **arg) {
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

static bool opts_add_send(bool (*next)(const char *, struct flow *, void *), struct flow *flow, const char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}
	return next(arg, flow, serializer);
}

bool opts_add_connect_receive(const char *arg) {
	return opts_add_connect(arg, receive_flow, NULL);
}

bool opts_add_connect_send(const char *arg) {
	return opts_add_send(opts_add_connect, send_flow, arg);
}

bool opts_add_connect_send_receive(const char *arg) {
	return opts_add_send(opts_add_connect, send_receive_flow, arg);
}

bool opts_add_listen_receive(const char *arg) {
	return opts_add_listen(arg, receive_flow, NULL);
}

bool opts_add_listen_send(const char *arg) {
	return opts_add_send(opts_add_listen, send_flow, arg);
}

bool opts_add_listen_send_receive(const char *arg) {
	return opts_add_send(opts_add_listen, send_receive_flow, arg);
}

bool opts_add_file_read(const char *arg) {
	file_read_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_file_write(const char *arg) {
	return opts_add_send(opts_add_file_write_int, send_flow, arg);
}

bool opts_add_file_write_read(const char *arg) {
	return opts_add_send(opts_add_file_write_int, send_receive_flow, arg);
}

bool opts_add_file_append(const char *arg) {
	return opts_add_send(opts_add_file_append_int, send_flow, arg);
}

bool opts_add_file_append_read(const char *arg) {
	return opts_add_send(opts_add_file_append_int, send_receive_flow, arg);
}

bool opts_add_exec_receive(const char *arg) {
	exec_new(arg, receive_flow, NULL);
	return true;
}

bool opts_add_exec_send(const char *arg) {
	return opts_add_send(opts_add_exec, send_flow, arg);
}

bool opts_add_exec_send_receive(const char *arg) {
	return opts_add_send(opts_add_exec, send_receive_flow, arg);
}

bool opts_add_stdin(const char __attribute__((unused)) *arg) {
	int fd = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, receive_flow, NULL, NULL);
}

bool opts_add_stdout(const char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	int fd = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, send_flow, serializer, NULL);
}
