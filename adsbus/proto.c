#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "packet.h"
#include "server.h"

#include "adsb.pb-c.h"
#include "proto.h"

#define PROTO_MAGIC "aDsB"

static void proto_obj_to_buf(Adsb *wrapper, struct buf *buf) {
	assert(!buf->length);
	assert(adsb__get_packed_size(wrapper) <= BUF_LEN_MAX);
	buf->length = adsb__pack(wrapper, (uint8_t *) buf_at(buf, 0));
	assert(buf->length);
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
	wrapper.mode_s_short = &packet_out;
	proto_obj_to_buf(&wrapper, buf);
}

bool proto_parse(struct buf *buf, struct packet *packet, void *state_in) {
	Adsb *wrapper = adsb__unpack(NULL, buf->length, (uint8_t *) buf_at(buf, 0));
	if (!wrapper) {
		return false;
	}
	if (!wrapper->header &&
			!wrapper->mode_s_short &&
			!wrapper->mode_s_long) {
		// "oneof" is actually "zero or oneof"
		adsb__free_unpacked(wrapper, NULL);
		return false;
	}

	buf_consume(buf, adsb__get_packed_size(wrapper));
	adsb__free_unpacked(wrapper, NULL);
	packet->type = PACKET_TYPE_NONE; // XXX
	return true;
}

void proto_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		AdsbHeader header = ADSB_HEADER__INIT;
		header.magic = PROTO_MAGIC;
		header.server_version = server_version;
		header.server_id = server_id;
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
