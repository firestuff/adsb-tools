#include <stdbool.h>
#include <string.h>

#include "buf.h"
#include "log.h"
#include "packet.h"
#include "server.h"

#include "adsb.pb-c.h"
#include "proto.h"

#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wpacked"

#define PROTO_MAGIC "aDsB"

struct proto_parser_state {
	struct packet_mlat_state mlat_state;
	uint16_t mlat_timestamp_mhz;
	uint64_t mlat_timestamp_max;
	uint32_t rssi_max;
	bool have_header;
};

static Adsb *proto_prev = NULL;
static struct buf proto_hello_buf = BUF_INIT;

static void proto_obj_to_buf(ProtobufCMessage *obj, struct buf *buf) {
	assert(!buf->length);
	assert(protobuf_c_message_get_packed_size(obj) <= BUF_LEN_MAX);
	buf->length = protobuf_c_message_pack(obj, buf_at(buf, 0));
	assert(buf->length);
}

static void proto_wrap_to_buf(Adsb *msg, struct buf *buf) {
	AdsbStream wrapper = ADSB_STREAM__INIT;
	wrapper.n_msg = 1;
	wrapper.msg = &msg;
	proto_obj_to_buf((struct ProtobufCMessage *) &wrapper, buf);
}

static void proto_serialize_packet(struct packet *packet, AdsbPacket *out, size_t len) {
	out->source_id = (char *) packet->source_id;
	out->hops = packet->hops;
	if (packet->mlat_timestamp) {
		out->mlat_timestamp = packet->mlat_timestamp;
		out->has_mlat_timestamp = true;
	}
	if (packet->rssi) {
		out->rssi = packet->rssi;
		out->has_rssi = true;
	}
	out->payload.data = packet->payload;
	out->payload.len = len;
}

static void proto_serialize_mode_ac(struct packet *packet, struct buf *buf) {
	AdsbPacket packet_out = ADSB_PACKET__INIT;
	proto_serialize_packet(packet, &packet_out, 2);
	Adsb msg = ADSB__INIT;
	msg.mode_ac = &packet_out;
	proto_wrap_to_buf(&msg, buf);
}

static void proto_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	AdsbPacket packet_out = ADSB_PACKET__INIT;
	proto_serialize_packet(packet, &packet_out, 7);
	Adsb msg = ADSB__INIT;
	msg.mode_s_short = &packet_out;
	proto_wrap_to_buf(&msg, buf);
}

static void proto_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	AdsbPacket packet_out = ADSB_PACKET__INIT;
	proto_serialize_packet(packet, &packet_out, 14);
	Adsb msg = ADSB__INIT;
	msg.mode_s_long = &packet_out;
	proto_wrap_to_buf(&msg, buf);
}

static ssize_t proto_parse_varint(const struct buf *buf, size_t *start) {
	uint64_t value = 0;
	uint16_t shift = 0;
	for (; *start < buf->length; (*start)++) {
		uint8_t c = buf_chr(buf, *start);
		value |= ((uint64_t) c & 0x7f) << shift;
		if (value > SSIZE_MAX) {
			return -1;
		}
		if (!(c & 0x80)) {
			(*start)++;
			return (ssize_t) value;
		}
		shift += 7;
		if (shift > 21) {
			return -1;
		}
	}
	return -1;
}

static ssize_t proto_unwrap(const struct buf *wrapper, Adsb **msg) {
	if (wrapper->length < 1) {
		return -1;
	}

	// Field ID 1, encoding type 2 (length-prefixed blob)
	if (buf_chr(wrapper, 0) != ((1 << 3) | 2)) {
		return -1;
	}

	size_t start = 1;
	ssize_t len = proto_parse_varint(wrapper, &start);
	if (len == -1) {
		return -1;
	}
	size_t msg_len = (size_t) len;
	if (start + msg_len > wrapper->length) {
		return -1;
	}

	*msg = adsb__unpack(NULL, msg_len, buf_at(wrapper, start));
	if (!*msg) {
		return -1;
	}

	return (ssize_t) (start + msg_len);
}

static bool proto_parse_header(AdsbHeader *header, struct packet *packet, struct proto_parser_state *state) {
	if (strcmp(header->magic, PROTO_MAGIC)) {
		return false;
	}

	if (!header->mlat_timestamp_mhz ||
			header->mlat_timestamp_mhz > UINT16_MAX ||
			!header->mlat_timestamp_max ||
			!header->rssi_max) {
		return false;
	}
	state->mlat_timestamp_mhz = (uint16_t) header->mlat_timestamp_mhz;
	state->mlat_timestamp_max = header->mlat_timestamp_max;
	state->rssi_max = header->rssi_max;

	if (!strcmp(header->server_id, (const char *) server_id)) {
		log_write('R', packet->source_id, "Attempt to receive proto data from our own server ID (%s); loop!", server_id);
		return false;
	}

	state->have_header = true;
	log_write('R', packet->source_id, "Connected to server ID: %s", header->server_id);
	return true;
}

static bool proto_parse_packet(AdsbPacket *in, struct packet *packet, struct proto_parser_state *state, size_t len) {
	if (!state->have_header) {
		return false;
	}

	if (in->payload.len != len) {
		return false;
	}

	if (!packet_validate_id((const uint8_t *) in->source_id)) {
		return false;
	}

	packet->source_id = (uint8_t *) in->source_id;
	packet->hops = (uint16_t) in->hops;
	memcpy(packet->payload, in->payload.data, len);

	if (in->has_mlat_timestamp) {
		packet->mlat_timestamp = packet_mlat_timestamp_scale_in(
				in->mlat_timestamp,
				state->mlat_timestamp_max,
				state->mlat_timestamp_mhz,
				&state->mlat_state);
	}

	if (in->has_rssi) {
		packet->rssi = packet_rssi_scale_in(in->rssi, state->rssi_max);
	}

	return true;
}

void proto_init() {
	AdsbHeader header = ADSB_HEADER__INIT;
	header.magic = PROTO_MAGIC;
	header.server_version = server_version;
	header.server_id = (char *) server_id;
	header.mlat_timestamp_mhz = PACKET_MLAT_MHZ;
	header.mlat_timestamp_max = PACKET_MLAT_MAX;
	header.rssi_max = PACKET_RSSI_MAX;

	Adsb msg = ADSB__INIT;
	msg.header = &header;

	proto_wrap_to_buf(&msg, &proto_hello_buf);
}

void proto_cleanup() {
	if (proto_prev) {
		adsb__free_unpacked(proto_prev, NULL);
	}
}

bool proto_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct proto_parser_state *state = (struct proto_parser_state *) state_in;

	if (proto_prev) {
		adsb__free_unpacked(proto_prev, NULL);
		proto_prev = NULL;
	}

	Adsb *msg;
	ssize_t len = proto_unwrap(buf, &msg);
	if (len == -1) {
		return false;
	}

	if (msg->header) {
		if (!proto_parse_header(msg->header, packet, state)) {
			adsb__free_unpacked(msg, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_NONE;
	} else if (msg->mode_ac) {
		if (!proto_parse_packet(msg->mode_ac, packet, state, 2)) {
			adsb__free_unpacked(msg, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_MODE_AC;
	} else if (msg->mode_s_short) {
		if (!proto_parse_packet(msg->mode_s_short, packet, state, 7)) {
			adsb__free_unpacked(msg, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_MODE_S_SHORT;
	} else if (msg->mode_s_long) {
		if (!proto_parse_packet(msg->mode_s_long, packet, state, 14)) {
			adsb__free_unpacked(msg, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_MODE_S_LONG;
	} else {
		// "oneof" is actually "zero or oneof"
		adsb__free_unpacked(msg, NULL);
		return false;
	}

	proto_prev = msg;
	buf_consume(buf, (size_t) len);
	return true;
}

void proto_serialize(struct packet *packet, struct buf *buf) {
	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_AC:
			proto_serialize_mode_ac(packet, buf);
			break;


		case PACKET_TYPE_MODE_S_SHORT:
			proto_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			proto_serialize_mode_s_long(packet, buf);
			break;
	}
}

void proto_hello(struct buf **buf) {
	*buf = &proto_hello_buf;
}
