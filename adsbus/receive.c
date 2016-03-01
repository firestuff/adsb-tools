#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "buf.h"
#include "flow.h"
#include "json.h"
#include "packet.h"
#include "peer.h"
#include "proto.h"
#include "raw.h"
#include "socket.h"
#include "send.h"
#include "uuid.h"

#include "receive.h"

struct receive;
typedef bool (*parser_wrapper)(struct receive *, struct packet *);
typedef bool (*parser)(struct buf *, struct packet *, void *state);
struct receive {
	struct peer peer;
	struct peer *on_close;
	uint8_t id[UUID_LEN];
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser_wrapper parser_wrapper;
	parser parser;
	struct receive *prev;
	struct receive *next;
};
static struct receive *receive_head = NULL;

static struct flow _receive_flow = {
	.name = "receive",
	.new = receive_new,
	.ref_count = &peer_count_in,
};
struct flow *receive_flow = &_receive_flow;

static struct parser {
	char *name;
	parser parse;
} parsers[] = {
	{
		.name = "airspy_adsb",
		.parse = airspy_adsb_parse,
	},
	{
		.name = "beast",
		.parse = beast_parse,
	},
	{
		.name = "json",
		.parse = json_parse,
	},
	{
		.name = "proto",
		.parse = proto_parse,
	},
	{
		.name = "raw",
		.parse = raw_parse,
	},
};
#define NUM_PARSERS (sizeof(parsers) / sizeof(*parsers))

static bool receive_parse_wrapper(struct receive *receive, struct packet *packet) {
	return receive->parser(&receive->buf, packet, receive->parser_state);
}

static bool receive_autodetect_parse(struct receive *receive, struct packet *packet) {
	struct buf *buf = &receive->buf;
	void *state = receive->parser_state;

	for (size_t i = 0; i < NUM_PARSERS; i++) {
		if (parsers[i].parse(buf, packet, state)) {
			fprintf(stderr, "R %s: Detected input format %s\n", receive->id, parsers[i].name);
			receive->parser_wrapper = receive_parse_wrapper;
			receive->parser = parsers[i].parse;
			return true;
		}
	}
	return false;
}

static void receive_del(struct receive *receive) {
	fprintf(stderr, "R %s: Connection closed\n", receive->id);
	peer_count_in--;
	peer_epoll_del((struct peer *) receive);
	assert(!close(receive->peer.fd));
	if (receive->prev) {
		receive->prev->next = receive->next;
	} else {
		receive_head = receive->next;
	}
	if (receive->next) {
		receive->next->prev = receive->prev;
	}
	peer_call(receive->on_close);
	free(receive);
}

static void receive_read(struct peer *peer) {
	struct receive *receive = (struct receive *) peer;

	if (buf_fill(&receive->buf, receive->peer.fd) <= 0) {
		receive_del(receive);
		return;
	}

	struct packet packet = {
		.source_id = receive->id,
	};
	while (receive->buf.length && receive->parser_wrapper(receive, &packet)) {
		if (packet.type == PACKET_TYPE_NONE) {
			continue;
		}
		send_write(&packet);
	}

	if (receive->buf.length == BUF_LEN_MAX) {
		fprintf(stderr, "R %s: Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.\n", receive->id);
		receive_del(receive);
		return;
	}
}

void receive_cleanup() {
	while (receive_head) {
		receive_del(receive_head);
	}
}

void receive_new(int fd, void __attribute__((unused)) *passthrough, struct peer *on_close) {
	peer_count_in++;

	socket_receive_init(fd);

	struct receive *receive = malloc(sizeof(*receive));
	assert(receive);

	receive->peer.fd = fd;
	receive->peer.event_handler = receive_read;
	receive->on_close = on_close;
	uuid_gen(receive->id);
	buf_init(&receive->buf);
	memset(receive->parser_state, 0, PARSER_STATE_LEN);
	receive->parser_wrapper = receive_autodetect_parse;
	receive->prev = NULL;
	receive->next = receive_head;
	if (receive->next) {
		receive->next->prev = receive;
	}
	receive_head = receive;
	peer_epoll_add((struct peer *) receive, EPOLLIN);

	fprintf(stderr, "R %s: New receive connection\n", receive->id);
}

void receive_print_usage() {
	fprintf(stderr, "\nSupported receive formats (auto-detected):\n");
	for (size_t i = 0; i < NUM_PARSERS; i++) {
		fprintf(stderr, "\t%s\n", parsers[i].name);
	}
}
