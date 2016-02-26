#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void opts_add_listen(char *host_port, incoming_connection_handler handler, void *passthrough, uint32_t *count) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, handler, passthrough, count);
		free(host);
	} else {
		incoming_new(NULL, host_port, handler, passthrough, count);
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

	outgoing_new(host, arg, receive_new, NULL, &peer_count_in);
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

	outgoing_new(host, arg, send_new_wrapper, serializer, &peer_count_out);
	free(host);
	return true;
}

bool opts_add_listen_receive(char *arg) {
	opts_add_listen(arg, receive_new, NULL, &peer_count_in);
	return true;
}

bool opts_add_listen_send(char *arg) {
	struct serializer *serializer = opts_get_serializer(&arg);
	if (!serializer) {
		return false;
	}

	opts_add_listen(arg, send_new_wrapper, serializer, &peer_count_out);
	return true;
}

bool opts_add_stdin(char __attribute__((unused)) *arg) {
	receive_new(dup(0), NULL, NULL);
	return true;
}

bool opts_add_stdout(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	send_new(dup(1), serializer, NULL);
	return true;
}
