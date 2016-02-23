#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "buf.h"
#include "packet.h"
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
	struct packet_mlat_state mlat_state;
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

static void beast_write_mlat(uint64_t timestamp, uint8_t *mlat_timestamp) {
	mlat_timestamp[0] = (timestamp >> 40) & 0xff;
	mlat_timestamp[1] = (timestamp >> 32) & 0xff;
	mlat_timestamp[2] = (timestamp >> 24) & 0xff;
	mlat_timestamp[3] = (timestamp >> 16) & 0xff;
	mlat_timestamp[4] = (timestamp >> 8) & 0xff;
	mlat_timestamp[5] = (timestamp) & 0xff;
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

static void beast_escape(struct buf *out, const struct buf *in) {
	for (int i = 0; i < in->length; i++, out->length++) {
		buf_chr(out, out->length) = buf_chr(in, i);
		if (i > 0 && buf_chr(in, i) == 0x1a) {
			buf_chr(out, ++(out->length)) = 0x1a;
		}
	}
}

static bool beast_parse_mode_s_short(struct buf *buf, struct packet *packet, struct beast_parser_state *state) {
	struct buf buf2 = BUF_INIT;
	ssize_t in_bytes = beast_unescape(&buf2, buf, sizeof(struct beast_mode_s_short_overlay));
	if (in_bytes < 0) {
		return false;
	}
	struct beast_mode_s_short_overlay *overlay = (struct beast_mode_s_short_overlay *) buf_at(&buf2, 0);
	packet->type = PACKET_TYPE_MODE_S_SHORT;
	uint64_t source_mlat = beast_parse_mlat(overlay->mlat_timestamp);
	packet->mlat_timestamp = packet_mlat_timestamp_scale_in(source_mlat, UINT64_C(0xffffffffffff), 12, &state->mlat_state);
	packet->rssi = packet_rssi_scale_in(overlay->rssi, UINT8_MAX);
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
	packet->type = PACKET_TYPE_MODE_S_LONG;
	uint64_t source_mlat = beast_parse_mlat(overlay->mlat_timestamp);
	packet->mlat_timestamp = packet_mlat_timestamp_scale_in(source_mlat, UINT64_C(0xffffffffffff), 12, &state->mlat_state);
	packet->rssi = packet_rssi_scale_in(overlay->rssi == UINT8_MAX ? 0 : overlay->rssi, UINT8_MAX);
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
		case 0x32:
			return beast_parse_mode_s_short(buf, packet, state);

		case 0x33:
			return beast_parse_mode_s_long(buf, packet, state);

		default:
			fprintf(stderr, "R %s: Unknown beast type %x\n", packet->source_id, overlay->type);
			return false;
	}
	return false;
}

void beast_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	struct buf buf2 = BUF_INIT;
	struct beast_mode_s_short_overlay *overlay = (struct beast_mode_s_short_overlay *) buf_at(&buf2, 0);
	overlay->common.one_a = 0x1a;
	overlay->common.type = 0x32;
	memcpy(overlay->payload, packet->payload, sizeof(overlay->payload));
	beast_write_mlat(
			packet_mlat_timestamp_scale_out(packet->mlat_timestamp, UINT64_C(0xffffffffffff), 12),
			overlay->mlat_timestamp);

	if (packet->rssi) {
		overlay->rssi = packet_rssi_scale_out(packet->rssi, UINT8_MAX);
	} else {
		overlay->rssi = UINT8_MAX;
	}

	buf2.length = sizeof(*overlay);
	beast_escape(buf, &buf2);
}

void beast_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	struct buf buf2 = BUF_INIT;
	struct beast_mode_s_long_overlay *overlay = (struct beast_mode_s_long_overlay *) buf_at(&buf2, 0);
	overlay->common.one_a = 0x1a;
	overlay->common.type = 0x33;
	memcpy(overlay->payload, packet->payload, sizeof(overlay->payload));
	beast_write_mlat(
			packet_mlat_timestamp_scale_out(packet->mlat_timestamp, UINT64_C(0xffffffffffff), 12),
			overlay->mlat_timestamp);

	if (packet->rssi) {
		overlay->rssi = packet_rssi_scale_out(packet->rssi, UINT8_MAX);
	} else {
		overlay->rssi = UINT8_MAX;
	}

	buf2.length = sizeof(*overlay);
	beast_escape(buf, &buf2);
}

void beast_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		return;
	}

	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			beast_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			beast_serialize_mode_s_long(packet, buf);
			break;
	}
}
