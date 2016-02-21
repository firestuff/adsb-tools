#include <stdlib.h>
#include <string.h>

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

bool opts_add_dump(char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	send_add(1, serializer);
	return true;
}

bool opts_add_connect_receive(char *arg) {
	char *host = opts_split(&arg, '/');
	if (!host) {
		return false;
	}

	outgoing_new(host, arg, receive_new, NULL);
	free(host);
	return true;
}

bool opts_add_connect_send(char *arg) {
	char *format = opts_split(&arg, '=');
	if (!format) {
		return false;
	}

	struct serializer *serializer = send_get_serializer(format);
	if (!serializer) {
		free(format);
		return false;
	}

	char *host = opts_split(&arg, '/');
	if (!host) {
		free(format);
		return false;
	}

	incoming_new(host, arg, send_add_wrapper, serializer);
	free(format);
	free(host);
	return true;
}

bool opts_add_listen_receive(char *arg) {
	char *host = opts_split(&arg, '/');
	if (host) {
		incoming_new(host, arg, receive_new, NULL);
		free(host);
	} else {
		incoming_new(NULL, arg, receive_new, NULL);
	}
	return true;
}

bool opts_add_listen_send(char *arg) {
	char *format = opts_split(&arg, '=');
	if (!format) {
		return false;
	}

	struct serializer *serializer = send_get_serializer(format);
	if (!serializer) {
		free(format);
		return false;
	}

	char *host = opts_split(&arg, '/');
	if (host) {
		incoming_new(host, arg, send_add_wrapper, serializer);
		free(host);
	} else {
		incoming_new(NULL, arg, send_add_wrapper, serializer);
	}
	return true;
}
