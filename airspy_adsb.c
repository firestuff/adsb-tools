#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

struct __attribute__((packed)) airspy_adsb_common_overlay {
	char mlat_timestamp[8];
	char semicolon1;
	char mlat_precision[2];
	char semicolon2;
	char rssi[4];
	char semicolon3;
};

struct __attribute__((packed)) airspy_adsb_mode_s_short_overlay {
	char asterisk;
	char payload[14];
	char semicolon;
	struct airspy_adsb_common_overlay common;
	char cr;
	char lf;
};

struct __attribute__((packed)) airspy_adsb_mode_s_long_overlay {
	char asterisk;
	char payload[28];
	char semicolon;
	struct airspy_adsb_common_overlay common;
	char cr;
	char lf;
};

struct airspy_adsb_parser_state {
	struct mlat_state mlat_state;
};

static bool airspy_adsb_parse_common(struct airspy_adsb_common_overlay *, struct packet *, struct airspy_adsb_parser_state *);


void airspy_adsb_init() {
	assert(sizeof(struct airspy_adsb_parser_state) <= PARSER_STATE_LEN);
}

static bool airspy_adsb_parse_mode_s_short(struct buf *buf, struct packet *packet, struct airspy_adsb_parser_state *state) {
	struct airspy_adsb_mode_s_short_overlay *short_overlay = (struct airspy_adsb_mode_s_short_overlay *) buf_at(buf, 0);
	if (buf->length < 35 ||
			short_overlay->asterisk != '*' ||
			short_overlay->semicolon != ';' ||
			short_overlay->cr != '\r' ||
			short_overlay->lf != '\n') {
		return false;
	}
	if (!airspy_adsb_parse_common(&short_overlay->common, packet, state)) {
		return false;
	}
	packet->type = MODE_S_SHORT;
	hex_to_bin(packet->payload, short_overlay->payload, sizeof(short_overlay->payload) / 2);
	buf_consume(buf, sizeof(*short_overlay));
	return true;
}

static bool airspy_adsb_parse_mode_s_long(struct buf *buf, struct packet *packet, struct airspy_adsb_parser_state *state) {
	struct airspy_adsb_mode_s_long_overlay *long_overlay = (struct airspy_adsb_mode_s_long_overlay *) buf_at(buf, 0);
	if (buf->length < 49 ||
			long_overlay->asterisk != '*' ||
			long_overlay->semicolon != ';' ||
			long_overlay->cr != '\r' ||
			long_overlay->lf != '\n') {
		return false;
	}
	if (!airspy_adsb_parse_common(&long_overlay->common, packet, state)) {
		return false;
	}
	packet->type = MODE_S_LONG;
	hex_to_bin(packet->payload, long_overlay->payload, sizeof(long_overlay->payload) / 2);
	buf_consume(buf, sizeof(*long_overlay));
	return true;
}

bool airspy_adsb_parse(struct backend *backend, struct packet *packet) {
	struct buf *buf = &backend->buf;
	struct airspy_adsb_parser_state *state = (struct airspy_adsb_parser_state *) backend->parser_state;

	return (
			airspy_adsb_parse_mode_s_short(buf, packet, state) ||
			airspy_adsb_parse_mode_s_long(buf, packet, state));
}

static bool airspy_adsb_parse_common(struct airspy_adsb_common_overlay *overlay, struct packet *packet, struct airspy_adsb_parser_state *state) {
	if (overlay->semicolon1 != ';' ||
	    overlay->semicolon2 != ';' ||
			overlay->semicolon3 != ';') {
		return false;
	}
	uint16_t mlat_mhz = 2 * hex_to_int(overlay->mlat_precision, sizeof(overlay->mlat_precision) / 2);
	packet->mlat_timestamp = mlat_timestamp_scale_in(hex_to_int(overlay->mlat_timestamp, sizeof(overlay->mlat_timestamp) / 2), UINT32_MAX, mlat_mhz, &state->mlat_state);
	packet->rssi = rssi_scale_in(hex_to_int(overlay->rssi, sizeof(overlay->rssi) / 2), UINT16_MAX);
	return true;
}
