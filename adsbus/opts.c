#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "buf.h"
#include "incoming.h"
#include "outgoing.h"
#include "peer.h"
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

static void opts_add_listen(char *host_port, incoming_connection_handler handler, incoming_get_hello hello, void *passthrough, uint32_t *count) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, handler, hello, passthrough, count);
		free(host);
	} else {
		incoming_new(NULL, host_port, handler, hello, passthrough, count);
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

	outgoing_new(host, arg, receive_new, NULL, NULL, &peer_count_in);
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

	outgoing_new(host, arg, send_new_wrapper, send_hello, serializer, &peer_count_out);
	free(host);
	return true;
}

bool opts_add_listen_receive(char *arg) {
	opts_add_listen(arg, receive_new, NULL, NULL, &peer_count_in);
	return true;
}

bool opts_add_listen_send(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	opts_add_listen(arg, send_new_wrapper, send_hello, serializer, &peer_count_out);
	return true;
}

bool opts_add_file_read(char *arg) {
	int fd = open(arg, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		return false;
	}
	receive_new(fd, NULL, NULL);
	return true;
}

bool opts_add_file_write(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return NULL;
	}

	int fd = open(arg, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC | O_CLOEXEC, S_IRWXU);
	if (fd == -1) {
		return false;
	}
	return send_new_hello(fd, serializer, NULL);
}

bool opts_add_file_append(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return NULL;
	}

	int fd = open(arg, O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC, S_IRWXU);
	if (fd == -1) {
		return false;
	}
	return send_new_hello(fd, serializer, NULL);
}

bool opts_add_stdin(char __attribute__((unused)) *arg) {
	int fd = dup(0);
	assert(!fcntl(fd, F_SETFD, FD_CLOEXEC));
	receive_new(fd, NULL, NULL);
	return true;
}

bool opts_add_stdout(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	int fd = dup(1);
	assert(!fcntl(fd, F_SETFD, FD_CLOEXEC));
	return send_new_hello(fd, serializer, NULL);
}
