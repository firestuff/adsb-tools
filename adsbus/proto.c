#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wpacked"

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "packet.h"
#include "server.h"
#include "uuid.h"

#include "adsb.pb-c.h"
#include "proto.h"

#define PROTO_MAGIC "aDsB"

struct __attribute__((packed)) proto_header {
	uint32_t length;
};

struct proto_parser_state {
	struct packet_mlat_state mlat_state;
	uint16_t mlat_timestamp_mhz;
	uint64_t mlat_timestamp_max;
	uint32_t rssi_max;
	bool have_header;
};

static Adsb *proto_prev = NULL;

static void proto_obj_to_buf(Adsb *wrapper, struct buf *buf) {
	assert(!buf->length);
	struct proto_header *header = (struct proto_header *) buf_at(buf, 0);
	assert(sizeof(*header) <= BUF_LEN_MAX);
	size_t msg_len = adsb__get_packed_size(wrapper);
	buf->length = sizeof(*header) + msg_len;
	assert(buf->length <= BUF_LEN_MAX);
	assert(adsb__pack(wrapper, (uint8_t *) buf_at(buf, sizeof(*header))) == msg_len);
	header->length = htonl(msg_len);
}

static void proto_serialize_packet(struct packet *packet, AdsbPacket *out, size_t len) {
	out->source_id = (char *) packet->source_id;
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

static void proto_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	AdsbPacket packet_out = ADSB_PACKET__INIT;
	proto_serialize_packet(packet, &packet_out, 7);
	Adsb wrapper = ADSB__INIT;
	wrapper.mode_s_short = &packet_out;
	proto_obj_to_buf(&wrapper, buf);
}

static void proto_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	AdsbPacket packet_out = ADSB_PACKET__INIT;
	proto_serialize_packet(packet, &packet_out, 14);
	Adsb wrapper = ADSB__INIT;
	wrapper.mode_s_long = &packet_out;
	proto_obj_to_buf(&wrapper, buf);
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
		fprintf(stderr, "R %s: Attempt to receive proto data from our own server ID (%s); loop!\n", packet->source_id, server_id);
		return false;
	}

	fprintf(stderr, "R %s: Connected to server ID: %s\n", packet->source_id, header->server_id);
	return true;
}

static bool proto_parse_packet(AdsbPacket *in, struct packet *packet, struct proto_parser_state *state, size_t len) {
	if (in->payload.len != len) {
		return false;
	}

	if (!packet_validate_id((const uint8_t *) in->source_id)) {
		return false;
	}
	packet->source_id = (uint8_t *) in->source_id;
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

	struct proto_header *header = (struct proto_header *) buf_at(buf, 0);
	if (buf->length < sizeof(*header)) {
		return false;
	}
	uint32_t msg_len = ntohl(header->length);
	if (buf->length < sizeof(*header) + msg_len) {
		return false;
	}

	Adsb *wrapper = adsb__unpack(NULL, msg_len, (uint8_t *) buf_at(buf, sizeof(*header)));
	if (!wrapper) {
		return false;
	}

	if (wrapper->header) {
		if (!proto_parse_header(wrapper->header, packet, state)) {
			adsb__free_unpacked(wrapper, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_NONE;
	} else if (wrapper->mode_s_short) {
		if (!proto_parse_packet(wrapper->mode_s_short, packet, state, 7)) {
			adsb__free_unpacked(wrapper, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_MODE_S_SHORT;
	} else if (wrapper->mode_s_long) {
		if (!proto_parse_packet(wrapper->mode_s_long, packet, state, 14)) {
			adsb__free_unpacked(wrapper, NULL);
			return false;
		}
		packet->type = PACKET_TYPE_MODE_S_LONG;
	} else {
		// "oneof" is actually "zero or oneof"
		adsb__free_unpacked(wrapper, NULL);
		return false;
	}

	proto_prev = wrapper;
	buf_consume(buf, sizeof(*header) + msg_len);
	return true;
}

void proto_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		AdsbHeader header = ADSB_HEADER__INIT;
		header.magic = PROTO_MAGIC;
		header.server_version = server_version;
		header.server_id = (char *) server_id;
		header.mlat_timestamp_mhz = PACKET_MLAT_MHZ;
		header.mlat_timestamp_max = PACKET_MLAT_MAX;
		header.rssi_max = PACKET_RSSI_MAX;

		Adsb wrapper = ADSB__INIT;
		wrapper.header = &header;

		proto_obj_to_buf(&wrapper, buf);
		return;
	}

	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			proto_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			proto_serialize_mode_s_long(packet, buf);
			break;
	}
}
