#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "buf.h"
#include "flow.h"
#include "json.h"
#include "log.h"
#include "opts.h"
#include "packet.h"
#include "peer.h"
#include "proto.h"
#include "raw.h"
#include "socket.h"
#include "stats.h"
#include "uuid.h"

#include "send.h"

struct send {
	struct peer peer;
	struct stat stat;
	struct peer *on_close;
	uint8_t id[UUID_LEN];
	struct serializer *serializer;
	struct list_head send_list;
};

static void send_new(int, void *, struct peer *);

static struct flow _send_flow = {
	.name = "send",
	.socket_ready = socket_ready_send,
	.socket_connected = socket_connected_send,
	.new = send_new,
	.get_hello = send_get_hello,
	.ref_count = &peer_count_out,
};
struct flow *send_flow = &_send_flow;

static char log_module = 'S';

typedef void (*serialize)(struct packet *, struct buf *);
typedef void (*hello)(struct buf **);
static struct serializer {
	char *name;
	serialize serialize;
	hello hello;
	struct list_head send_head;
} serializers[] = {
	{
		.name = "airspy_adsb",
		.serialize = airspy_adsb_serialize,
		.hello = NULL,
	},
	{
		.name = "beast",
		.serialize = beast_serialize,
		.hello = NULL,
	},
	{
		.name = "json",
		.serialize = json_serialize,
		.hello = json_hello,
	},
	{
		.name = "proto",
		.serialize = proto_serialize,
		.hello = proto_hello,
	},
	{
		.name = "raw",
		.serialize = raw_serialize,
		.hello = NULL,
	},
	{
		.name = "stats",
		.serialize = stats_serialize,
		.hello = NULL,
	},
};
#define NUM_SERIALIZERS (sizeof(serializers) / sizeof(*serializers))

static void send_del(struct send *send) {
	LOG(send->id, "Connection closed");
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

static void send_new(int fd, void *passthrough, struct peer *on_close) {
	struct serializer *serializer = (struct serializer *) passthrough;

	peer_count_out++;

	struct send *send = malloc(sizeof(*send));
	assert(send);

	send->peer.fd = fd;
	send->peer.event_handler = send_del_wrapper;
	send->on_close = on_close;
	uuid_gen(send->id);
	send->serializer = serializer;
	assert(!fstat(fd, &send->stat));

	list_add(&send->send_list, &serializer->send_head);

	peer_epoll_add((struct peer *) send, 0);

	LOG(send->id, "New send connection: %s", serializer->name);
}

static struct serializer *send_parse_format(const char **arg) {
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

void *send_get_serializer(const char *name) {
	for (size_t i = 0; i < NUM_SERIALIZERS; i++) {
		if (strcasecmp(serializers[i].name, name) == 0) {
			return &serializers[i];
		}
	}
	return NULL;
}

void send_get_hello(struct buf **buf_pp, void *passthrough) {
	struct serializer *serializer = (struct serializer *) passthrough;
	if (serializer->hello) {
		serializer->hello(buf_pp);
	}
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
			if (iter->stat.st_dev == packet->input_stat->st_dev &&
					iter->stat.st_ino == packet->input_stat->st_ino) {
				// Same socket that this packet came from
				continue;
			}
			if (write(iter->peer.fd, buf_at(&buf, 0), buf.length) != (ssize_t) buf.length) {
				// peer_loop() will see this shutdown and call send_del
				// Ignore error
				shutdown(iter->peer.fd, SHUT_WR);
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

bool send_add(bool (*next)(const char *, struct flow *, void *), struct flow *flow, const char *arg) {
	struct serializer *serializer = send_parse_format(&arg);
	if (!serializer) {
		return false;
	}
	return next(arg, flow, serializer);
}
