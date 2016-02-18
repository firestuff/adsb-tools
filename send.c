#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"

#include "send.h"

#include "beast.h"
#include "json.h"
#include "stats.h"

struct send {
	struct peer peer;
	char id[UUID_LEN];
	struct serializer *serializer;
	struct send *prev;
	struct send *next;
};

typedef void (*serializer)(struct packet *, struct buf *);
struct serializer {
	char *name;
	serializer serialize;
	struct send *send_head;
} serializers[] = {
	{
		.name = "beast",
		.serialize = beast_serialize,
	},
	{
		.name = "json",
		.serialize = json_serialize,
	},
	{
		.name = "stats",
		.serialize = stats_serialize,
	},
};
#define NUM_SERIALIZERS (sizeof(serializers) / sizeof(*serializers))

static void send_hangup(struct send *send) {
	fprintf(stderr, "S %s (%s): Peer disconnected\n", send->id, send->serializer->name);
	if (send->prev) {
		send->prev->next = send->next;
	} else {
		send->serializer->send_head = send->next;
	}
	if (send->next) {
		send->next->prev = send->prev;
	}
	close(send->peer.fd);
	free(send);
}

static void send_hangup_wrapper(struct peer *peer) {
	send_hangup((struct send *) peer);
}

static bool send_hello(int fd, struct serializer *serializer) {
	struct buf buf = BUF_INIT;
	serializer->serialize(NULL, &buf);
	if (buf.length == 0) {
		return true;
	}
	if (write(fd, buf_at(&buf, 0), buf.length) != buf.length) {
		return false;
	}
	return true;
}

void send_init() {
	signal(SIGPIPE, SIG_IGN);
}

struct serializer *send_get_serializer(char *name) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		if (strcasecmp(serializers[i].name, name) == 0) {
			return &serializers[i];
		}
	}
	return NULL;
}

void send_add(int fd, struct serializer *serializer) {
	int flags = fcntl(fd, F_GETFL, 0);
	assert(flags >= 0);
	flags |= O_NONBLOCK;
	assert(fcntl(fd, F_SETFL, flags) == 0);

	if (!send_hello(fd, serializer)) {
		fprintf(stderr, "S xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx: Failed to write hello\n");
		return;
	}

	struct send *send = malloc(sizeof(*send));
	assert(send);

	send->peer.fd = fd;
	send->peer.event_handler = send_hangup_wrapper;

	uuid_gen(send->id);
	send->serializer = serializer;
	send->prev = NULL;
	send->next = serializer->send_head;
	serializer->send_head = send;

	// Only listen for hangup
	peer_epoll_add((struct peer *) send, EPOLLIN);

	fprintf(stderr, "S %s (%s): New send connection\n", send->id, serializer->name);
}

void send_add_wrapper(int fd, void *passthrough) {
	send_add(fd, (struct serializer *) passthrough);
}

void send_write(struct packet *packet) {
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		struct serializer *serializer = &serializers[i];
		if (serializer->send_head == NULL) {
			continue;
		}
		struct buf buf = BUF_INIT;
		serializer->serialize(packet, &buf);
		if (buf.length == 0) {
			continue;
		}
		struct send *send = serializer->send_head;
		while (send) {
			if (write(send->peer.fd, buf_at(&buf, 0), buf.length) == buf.length) {
				send = send->next;
			} else {
				struct send *next = send->next;
				send_hangup(send);
				send = next;
			}
		}
	}
}

void send_print_usage() {
	fprintf(stderr, "\nSupported send formats:\n");
	for (int i = 0; i < NUM_SERIALIZERS; i++) {
		fprintf(stderr, "\t%s\n", serializers[i].name);
	}
}
