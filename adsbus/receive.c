#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "buf.h"
#include "flow.h"
#include "json.h"
#include "log.h"
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
	struct stat stat;
	struct peer *on_close;
	uint8_t id[UUID_LEN];
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser_wrapper parser_wrapper;
	parser parser;
	struct list_head receive_list;
};
static struct list_head receive_head = LIST_HEAD_INIT(receive_head);

static char log_module = 'R';

static void receive_new(int, void *, struct peer *);

static struct flow _receive_flow = {
	.name = "receive",
	.socket_connected = socket_connected_receive,
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

static uint32_t receive_max_hops = 10;

static bool receive_parse_wrapper(struct receive *receive, struct packet *packet) {
	return receive->parser(&receive->buf, packet, receive->parser_state);
}

static bool receive_autodetect_parse(struct receive *receive, struct packet *packet) {
	struct buf *buf = &receive->buf;
	void *state = receive->parser_state;

	// We don't trust parsers not to scribble over the packet.
	struct packet orig_packet;
	memcpy(&orig_packet, packet, sizeof(orig_packet));

	for (size_t i = 0; i < NUM_PARSERS; i++) {
		if (parsers[i].parse(buf, packet, state)) {
			LOG(receive->id, "Detected input format: %s", parsers[i].name);
			receive->parser_wrapper = receive_parse_wrapper;
			receive->parser = parsers[i].parse;
			return true;
		}
		if (i < NUM_PARSERS - 1) {
			memcpy(packet, &orig_packet, sizeof(*packet));
		}
	}
	return false;
}

static void receive_del(struct receive *receive) {
	LOG(receive->id, "Connection closed");
	peer_count_in--;
	peer_close(&receive->peer);
	list_del(&receive->receive_list);
	peer_call(receive->on_close);
	free(receive);
}

static void receive_read(struct peer *peer) {
	struct receive *receive = (struct receive *) peer;

	if (buf_fill(&receive->buf, receive->peer.fd) <= 0) {
		receive_del(receive);
		return;
	}

	while (receive->buf.length) {
		struct packet packet = {
			.source_id = receive->id,
			.input_stat = &receive->stat,
		};
		if (!receive->parser_wrapper(receive, &packet)) {
			break;
		}
		if (packet.type == PACKET_TYPE_NONE) {
			continue;
		}
		if (++packet.hops > receive_max_hops) {
			LOG(receive->id, "Packet exceeded hop limit (%u > %u); dropping. You may have a loop in your configuration.", packet.hops, receive_max_hops);
			continue;
		}
		send_write(&packet);
	}

	if (receive->buf.length == BUF_LEN_MAX) {
		LOG(receive->id, "Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.");
		receive_del(receive);
		return;
	}
}

static void receive_new(int fd, void __attribute__((unused)) *passthrough, struct peer *on_close) {
	peer_count_in++;

	struct receive *receive = malloc(sizeof(*receive));
	assert(receive);

	receive->peer.fd = fd;
	receive->peer.event_handler = receive_read;
	receive->on_close = on_close;
	uuid_gen(receive->id);
	buf_init(&receive->buf);
	memset(receive->parser_state, 0, PARSER_STATE_LEN);
	receive->parser_wrapper = receive_autodetect_parse;
	assert(!fstat(fd, &receive->stat));

	list_add(&receive->receive_list, &receive_head);

	peer_epoll_add(&receive->peer, EPOLLIN);

	LOG(receive->id, "New receive connection");
}

void receive_init() {
	char *max_hops = getenv("ADSBUS_MAX_HOPS");
	if (max_hops) {
		char *end_ptr;
		unsigned long max_hops_ul = strtoul(max_hops, &end_ptr, 10);
		assert(max_hops[0] != '\0');
		assert(end_ptr[0] == '\0');
		assert(max_hops_ul <= UINT32_MAX);
		receive_max_hops = (uint32_t) max_hops_ul;
	}
}

void receive_cleanup() {
	struct receive *iter, *next;
	list_for_each_entry_safe(iter, next, &receive_head, receive_list) {
		receive_del(iter);
	}
}

void receive_print_usage() {
	fprintf(stderr, "\nSupported receive formats (auto-detected):\n");
	for (size_t i = 0; i < NUM_PARSERS; i++) {
		fprintf(stderr, "\t%s\n", parsers[i].name);
	}
}
