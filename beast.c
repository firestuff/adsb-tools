#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "receive.h"
#include "beast.h"

struct __attribute__((packed)) beast_common_overlay {
	uint8_t one_a;
	uint8_t type;
};

struct __attribute__((packed)) beast_mode_s_short_overlay {
	struct beast_common_overlay common;
	uint8_t mlat_timestamp[6];
	uint8_t rssi;
	uint8_t payload[7];
};

struct __attribute__((packed)) beast_mode_s_long_overlay {
	struct beast_common_overlay common;
	uint8_t mlat_timestamp[6];
	uint8_t rssi;
	uint8_t payload[14];
};

struct beast_parser_state {
	struct mlat_state mlat_state;
};

static uint64_t beast_parse_mlat(uint8_t *mlat_timestamp) {
	return (
			((uint64_t) mlat_timestamp[0]) << 40 |
			((uint64_t) mlat_timestamp[1]) << 32 |
			((uint64_t) mlat_timestamp[2]) << 24 |
			((uint64_t) mlat_timestamp[3]) << 16 |
			((uint64_t) mlat_timestamp[4]) << 8 |
			((uint64_t) mlat_timestamp[5]));
}

static ssize_t beast_unescape(struct buf *out, const struct buf *in, size_t out_bytes) {
	int o = 0, i = 0;
	for (; i < in->length && o < out_bytes; i++, o++) {
		if (i > 0 && buf_chr(in, i) == 0x1a) {
			if (i == in->length - 1 || buf_chr(in, i + 1) != 0x1a) {
				return -1;
			}
			i++;
		}
		buf_chr(out, o) = buf_chr(in, i);
	}
	if (o == out_bytes) {
		return i;
	} else {
		return -1;
	}
}

static bool beast_parse_mode_ac(struct buf *buf, struct packet *packet, struct beast_parser_state *state) {
	return false;
}

static bool beast_parse_mode_s_short(struct buf *buf, struct packet *packet, struct beast_parser_state *state) {
	struct buf buf2 = BUF_INIT;
	ssize_t in_bytes = beast_unescape(&buf2, buf, sizeof(struct beast_mode_s_short_overlay));
	if (in_bytes < 0) {
		return false;
	}
	struct beast_mode_s_short_overlay *overlay = (struct beast_mode_s_short_overlay *) buf_at(&buf2, 0);
	packet->type = MODE_S_SHORT;
	uint64_t source_mlat = beast_parse_mlat(overlay->mlat_timestamp);
	packet->mlat_timestamp = mlat_timestamp_scale_in(source_mlat, UINT64_C(0xffffffffffff), 12, &state->mlat_state);
	packet->rssi = rssi_scale_in(overlay->rssi == UINT8_MAX ? 0 : overlay->rssi, UINT8_MAX);
	memcpy(packet->payload, overlay->payload, sizeof(overlay->payload));
	buf_consume(buf, in_bytes);
	return true;
}

static bool beast_parse_mode_s_long(struct buf *buf, struct packet *packet, struct beast_parser_state *state) {
	struct buf buf2 = BUF_INIT;
	ssize_t in_bytes = beast_unescape(&buf2, buf, sizeof(struct beast_mode_s_long_overlay));
	if (in_bytes < 0) {
		return false;
	}
	struct beast_mode_s_long_overlay *overlay = (struct beast_mode_s_long_overlay *) buf_at(&buf2, 0);
	packet->type = MODE_S_LONG;
	uint64_t source_mlat = beast_parse_mlat(overlay->mlat_timestamp);
	packet->mlat_timestamp = mlat_timestamp_scale_in(source_mlat, UINT64_C(0xffffffffffff), 12, &state->mlat_state);
	packet->rssi = rssi_scale_in(overlay->rssi == UINT8_MAX ? 0 : overlay->rssi, UINT8_MAX);
	memcpy(packet->payload, overlay->payload, sizeof(overlay->payload));
	buf_consume(buf, in_bytes);
	return true;
}

void beast_init() {
	assert(sizeof(struct beast_parser_state) <= PARSER_STATE_LEN);
	assert(sizeof(struct beast_mode_s_short_overlay) * 2 <= BUF_LEN_MAX);
	assert(sizeof(struct beast_mode_s_long_overlay) * 2 <= BUF_LEN_MAX);
}

bool beast_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct beast_parser_state *state = (struct beast_parser_state *) state_in;

	if (buf->length < sizeof(struct beast_common_overlay) ||
			buf_chr(buf, 0) != 0x1a) {
		return false;
	}

	struct beast_common_overlay *overlay = (struct beast_common_overlay *) buf_at(buf, 0);
	switch (overlay->type) {
		case 0x31:
			return beast_parse_mode_ac(buf, packet, state);

		case 0x32:
			return beast_parse_mode_s_short(buf, packet, state);

		case 0x33:
			return beast_parse_mode_s_long(buf, packet, state);
	}
	return false;
}
