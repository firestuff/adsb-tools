#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

struct backend_state {
	uint64_t mlat_timestamp_last;
	uint64_t mlat_timestamp_generation;
};

static bool airspy_adsb_parse_common(char *, struct packet *, struct backend_state *);


void airspy_adsb_init() {
	assert(sizeof(struct backend_state) <= PARSER_STATE_LEN);
}

bool airspy_adsb_parse(struct buf *buf, struct packet *packet, void *state_in) {
	if (buf->length < 35 ||
	    buf_chr(buf, 0) != '*') {
		return false;
	}
	struct backend_state *state = state_in;
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

static bool airspy_adsb_parse_common(char *in, struct packet *packet, struct backend_state *state) {
	if (in[8] != ';' ||
	    in[11] != ';' ||
			in[16] != ';') {
		return false;
	}
	uint16_t mlat_mhz = 2 * hex_to_int(&in[9], 1);
	packet->mlat_timestamp = hex_to_int(in, 4) * (MLAT_MHZ / mlat_mhz);
	packet->rssi = hex_to_int(&in[12], 2) * (RSSI_MAX / UINT16_MAX);
	return true;
}
