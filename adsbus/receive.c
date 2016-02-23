#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "raw.h"

#include "send.h"

#include "uuid.h"

#include "receive.h"

struct receive;
typedef bool (*parser_wrapper)(struct receive *, struct packet *);
typedef bool (*parser)(struct buf *, struct packet *, void *state);
struct receive {
	struct peer peer;
	char id[UUID_LEN];
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser_wrapper parser_wrapper;
	parser parser;
};

struct parser {
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

	for (int i = 0; i < NUM_PARSERS; i++) {
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
	assert(!close(receive->peer.fd));
	free(receive);
}

static void receive_read(struct peer *peer) {
	struct receive *receive = (struct receive *) peer;

	if (buf_fill(&receive->buf, receive->peer.fd) <= 0) {
		fprintf(stderr, "R %s: Connection closed by peer\n", receive->id);
		receive_del(receive);
		return;
	}

	struct packet packet = { 0 };
	while (receive->parser_wrapper(receive, &packet)) {
		send_write(&packet);
	}

	if (receive->buf.length == BUF_LEN_MAX) {
		fprintf(stderr, "R %s: Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.\n", receive->id);
		receive_del(receive);
		return;
	}
}

void receive_new(int fd, void *unused) {
	struct receive *receive = malloc(sizeof(*receive));
	assert(receive);
	uuid_gen(receive->id);
	receive->peer.fd = fd;
	buf_init(&receive->buf);
	memset(receive->parser_state, 0, PARSER_STATE_LEN);
	receive->parser_wrapper = receive_autodetect_parse;
	receive->peer.event_handler = receive_read;
	peer_epoll_add((struct peer *) receive, EPOLLIN);

	fprintf(stderr, "R %s: New receive connection\n", receive->id);
}

void receive_print_usage() {
	fprintf(stderr, "\nSupported receive formats (auto-detected):\n");
	for (int i = 0; i < NUM_PARSERS; i++) {
		fprintf(stderr, "\t%s\n", parsers[i].name);
	}
}
