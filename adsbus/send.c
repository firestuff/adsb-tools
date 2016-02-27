#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "buf.h"
#include "json.h"
#include "list.h"
#include "packet.h"
#include "peer.h"
#include "proto.h"
#include "raw.h"
#include "stats.h"
#include "uuid.h"

#include "send.h"

struct send {
	struct peer peer;
	struct peer *on_close;
	uint8_t id[UUID_LEN];
	struct serializer *serializer;
	struct list_head send_list;
};

typedef void (*serialize)(struct packet *, struct buf *);
static struct serializer {
	char *name;
	serialize serialize;
	struct list_head send_head;
} serializers[] = {
	{
		.name = "airspy_adsb",
		.serialize = airspy_adsb_serialize,
	},
	{
		.name = "beast",
		.serialize = beast_serialize,
	},
	{
		.name = "json",
		.serialize = json_serialize,
	},
	{
		.name = "proto",
		.serialize = proto_serialize,
	},
	{
		.name = "raw",
		.serialize = raw_serialize,
	},
	{
		.name = "stats",
		.serialize = stats_serialize,
	},
};
#define NUM_SERIALIZERS (sizeof(serializers) / sizeof(*serializers))

static void send_del(struct send *send) {
	fprintf(stderr, "S %s (%s): Connection closed\n", send->id, send->serializer->name);
	peer_count_out--;
	peer_epoll_del((struct peer *) send);
	assert(!close(send->peer.fd));
	list_del(&send->send_list);
	peer_call(send->on_close);
	free(send);
}

static void send_del_wrapper(struct peer *peer) {
	send_del((struct send *) peer);
}

void send_init() {
	assert(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		list_head_init(&serializers[i].send_head);
	}
}

void send_cleanup() {
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		struct send *iter, *next;
		list_for_each_entry_safe(iter, next, &serializers[i].send_head, send_list) {
			send_del(iter);
		}
	}
}

struct serializer *send_get_serializer(char *name) {
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		if (strcasecmp(serializers[i].name, name) == 0) {
			return &serializers[i];
		}
	}
	return NULL;
}

void send_new(int fd, struct serializer *serializer, struct peer *on_close) {
	peer_count_out++;

	int res = shutdown(fd, SHUT_RD);
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));

	struct send *send = malloc(sizeof(*send));
	assert(send);

	send->peer.fd = fd;
	send->peer.event_handler = send_del_wrapper;
	send->on_close = on_close;
	uuid_gen(send->id);
	send->serializer = serializer;
	list_add(&send->send_list, &serializer->send_head);

	peer_epoll_add((struct peer *) send, 0);

	fprintf(stderr, "S %s (%s): New send connection\n", send->id, serializer->name);
}

void send_new_wrapper(int fd, void *passthrough, struct peer *on_close) {
	send_new(fd, (struct serializer *) passthrough, on_close);
}

void send_hello(struct buf **buf_pp, void *passthrough) {
	struct serializer *serializer = (struct serializer *) passthrough;
	// TODO: change API to avoid special-case NULL packet*, and to allow static greetings.
	serializer->serialize(NULL, *buf_pp);
}

void send_write(struct packet *packet) {
	packet_sanity_check(packet);
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		struct serializer *serializer = &serializers[i];
		if (list_is_empty(&serializer->send_head)) {
			continue;
		}
		struct buf buf = BUF_INIT;
		serializer->serialize(packet, &buf);
		if (buf.length == 0) {
			continue;
		}
		struct send *iter, *next;
		list_for_each_entry_safe(iter, next, &serializer->send_head, send_list) {
			if (write(iter->peer.fd, buf_at(&buf, 0), buf.length) != (ssize_t) buf.length) {
				// peer_loop() will see this shutdown and call send_del
				int res = shutdown(iter->peer.fd, SHUT_WR);
				assert(res == 0 || (res == -1 && errno == ENOTSOCK));
			}
		}
	}
}

void send_print_usage() {
	fprintf(stderr, "\nSupported send formats:\n");
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		fprintf(stderr, "\t%s\n", serializers[i].name);
	}
}
