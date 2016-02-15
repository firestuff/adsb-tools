#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

struct airspy_adsb_parser_state {
	struct mlat_state mlat_state;
};

static bool airspy_adsb_parse_common(char *, struct packet *, struct airspy_adsb_parser_state *);


void airspy_adsb_init() {
	assert(sizeof(struct airspy_adsb_parser_state) <= PARSER_STATE_LEN);
}

bool airspy_adsb_parse(struct backend *backend, struct packet *packet) {
	struct buf *buf = &backend->buf;
	struct airspy_adsb_parser_state *state = (struct airspy_adsb_parser_state *) backend->parser_state;

	if (buf->length < 35 ||
	    buf_chr(buf, 0) != '*') {
		return false;
	}
	if (buf->length >= 35 &&
	    buf_chr(buf, 33) == '\r' &&
	    buf_chr(buf, 34) == '\n' &&
			buf_chr(buf, 15) == ';') {
		if (!airspy_adsb_parse_common(buf_at(buf, 16), packet, state)) {
			return false;
		}
		packet->type = MODE_S_SHORT;
		hex_to_bin(packet->data, buf_at(buf, 1), 7);
		buf_consume(buf, 35);
		return true;
	}
	if (buf->length >= 49 &&
	    buf_chr(buf, 47) == '\r' &&
	    buf_chr(buf, 48) == '\n' &&
			buf_chr(buf, 29) == ';') {
		if (!airspy_adsb_parse_common(buf_at(buf, 30), packet, state)) {
			return false;
		}
		packet->type = MODE_S_LONG;
		hex_to_bin(packet->data, buf_at(buf, 1), 14);
		buf_consume(buf, 49);
		return true;
	}
	return false;
}

static bool airspy_adsb_parse_common(char *in, struct packet *packet, struct airspy_adsb_parser_state *state) {
	if (in[8] != ';' ||
	    in[11] != ';' ||
			in[16] != ';') {
		return false;
	}
	uint16_t mlat_mhz = 2 * hex_to_int(&in[9], 1);
	packet->mlat_timestamp = mlat_timestamp_scale_in(hex_to_int(in, 4), UINT32_MAX, mlat_mhz, &state->mlat_state);
	packet->rssi = rssi_scale_in(hex_to_int(&in[12], 2), UINT16_MAX);
	return true;
}
