#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "hex.h"
#include "packet.h"
#include "receive.h"
#include "uuid.h"

#include "airspy_adsb.h"

#define SEND_MHZ 20

struct __attribute__((packed)) airspy_adsb_common_overlay {
	uint8_t mlat_timestamp[8];
	char semicolon1;
	uint8_t mlat_precision[2];
	char semicolon2;
	uint8_t rssi[4];
	char semicolon3;
};

struct __attribute__((packed)) airspy_adsb_mode_s_short_overlay {
	char asterisk;
	uint8_t payload[14];
	char semicolon;
	struct airspy_adsb_common_overlay common;
	char cr;
	char lf;
};

struct __attribute__((packed)) airspy_adsb_mode_s_long_overlay {
	char asterisk;
	uint8_t payload[28];
	char semicolon;
	struct airspy_adsb_common_overlay common;
	char cr;
	char lf;
};

struct airspy_adsb_parser_state {
	struct packet_mlat_state mlat_state;
};

static bool airspy_adsb_parse_common(const struct airspy_adsb_common_overlay *overlay, struct packet *packet, struct airspy_adsb_parser_state *state) {
	if (overlay->semicolon1 != ';' ||
	    overlay->semicolon2 != ';' ||
			overlay->semicolon3 != ';') {
		return false;
	}
	uint16_t mlat_mhz = 2 * (uint16_t) hex_to_int(overlay->mlat_precision, sizeof(overlay->mlat_precision) / 2);
	if (!mlat_mhz) {
		return false;
	}
	packet->mlat_timestamp = packet_mlat_timestamp_scale_in(hex_to_int(overlay->mlat_timestamp, sizeof(overlay->mlat_timestamp) / 2), UINT32_MAX, mlat_mhz, &state->mlat_state);
	packet->rssi = packet_rssi_scale_in((uint32_t) hex_to_int(overlay->rssi, sizeof(overlay->rssi) / 2), UINT16_MAX);
	return true;
}

static bool airspy_adsb_parse_mode_s_short(struct buf *buf, struct packet *packet, struct airspy_adsb_parser_state *state) {
	struct airspy_adsb_mode_s_short_overlay *short_overlay = (struct airspy_adsb_mode_s_short_overlay *) buf_at(buf, 0);
	if (buf->length < sizeof(*short_overlay) ||
			short_overlay->asterisk != '*' ||
			short_overlay->semicolon != ';' ||
			short_overlay->cr != '\r' ||
			short_overlay->lf != '\n') {
		return false;
	}
	if (!airspy_adsb_parse_common(&short_overlay->common, packet, state)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_SHORT;
	hex_to_bin(packet->payload, short_overlay->payload, sizeof(short_overlay->payload) / 2);
	buf_consume(buf, sizeof(*short_overlay));
	return true;
}

static bool airspy_adsb_parse_mode_s_long(struct buf *buf, struct packet *packet, struct airspy_adsb_parser_state *state) {
	struct airspy_adsb_mode_s_long_overlay *long_overlay = (struct airspy_adsb_mode_s_long_overlay *) buf_at(buf, 0);
	if (buf->length < sizeof(*long_overlay) ||
			long_overlay->asterisk != '*' ||
			long_overlay->semicolon != ';' ||
			long_overlay->cr != '\r' ||
			long_overlay->lf != '\n') {
		return false;
	}
	if (!airspy_adsb_parse_common(&long_overlay->common, packet, state)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_LONG;
	hex_to_bin(packet->payload, long_overlay->payload, sizeof(long_overlay->payload) / 2);
	buf_consume(buf, sizeof(*long_overlay));
	return true;
}

static void airspy_adsb_fill_common(struct packet *packet, struct airspy_adsb_common_overlay *overlay) {
	overlay->semicolon1 = overlay->semicolon2 = overlay->semicolon3 = ';';
	hex_from_int_upper(
			overlay->mlat_timestamp,
			packet_mlat_timestamp_scale_out(packet->mlat_timestamp, UINT32_MAX, SEND_MHZ),
			sizeof(overlay->mlat_timestamp) / 2);
	hex_from_int_upper(overlay->mlat_precision, SEND_MHZ / 2, sizeof(overlay->mlat_precision) / 2);
	hex_from_int_upper(overlay->rssi, packet_rssi_scale_out(packet->rssi, UINT16_MAX), sizeof(overlay->rssi) / 2);
}

static void airspy_adsb_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	struct airspy_adsb_mode_s_short_overlay *overlay = (struct airspy_adsb_mode_s_short_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->cr = '\r';
	overlay->lf = '\n';
	hex_from_bin_upper(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);

	airspy_adsb_fill_common(packet, &overlay->common);

	buf->length = sizeof(*overlay);
}

static void airspy_adsb_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	struct airspy_adsb_mode_s_long_overlay *overlay = (struct airspy_adsb_mode_s_long_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->cr = '\r';
	overlay->lf = '\n';
	hex_from_bin_upper(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);

	airspy_adsb_fill_common(packet, &overlay->common);

	buf->length = sizeof(*overlay);
}

void airspy_adsb_init() {
	assert(sizeof(struct airspy_adsb_parser_state) <= PARSER_STATE_LEN);
	assert(sizeof(struct airspy_adsb_mode_s_short_overlay) < BUF_LEN_MAX);
	assert(sizeof(struct airspy_adsb_mode_s_long_overlay) < BUF_LEN_MAX);
}

bool airspy_adsb_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct airspy_adsb_parser_state *state = (struct airspy_adsb_parser_state *) state_in;

	return (
			airspy_adsb_parse_mode_s_short(buf, packet, state) ||
			airspy_adsb_parse_mode_s_long(buf, packet, state));
}

void airspy_adsb_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		return;
	}

	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			airspy_adsb_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			airspy_adsb_serialize_mode_s_long(packet, buf);
			break;
	}
}
