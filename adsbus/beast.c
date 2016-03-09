#include <assert.h>
#include <string.h>

#include "buf.h"
#include "packet.h"
#include "receive.h"

#include "beast.h"

struct __attribute__((packed)) beast_overlay {
	uint8_t one_a;
	uint8_t type;
	uint8_t mlat_timestamp[6];
	uint8_t rssi;
};

struct beast_parser_state {
	struct packet_mlat_state mlat_state;
};

#define BEAST_MLAT_MHZ 12

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
	size_t o = 0, i = 0;
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
		return (ssize_t) i;
	} else {
		return -1;
	}
}

static void beast_escape(struct buf *out, const struct buf *in) {
	for (size_t i = 0; i < in->length; i++, out->length++) {
		buf_chr(out, out->length) = buf_chr(in, i);
		if (i > 0 && buf_chr(in, i) == 0x1a) {
			buf_chr(out, ++(out->length)) = 0x1a;
		}
	}
}

static bool beast_parse_packet(struct buf *buf, struct packet *packet, struct beast_parser_state *state, enum packet_type type) {
	struct buf buf2 = BUF_INIT;
	size_t payload_bytes = packet_payload_len[type];
	ssize_t in_bytes = beast_unescape(&buf2, buf, sizeof(struct beast_overlay) + payload_bytes);
	if (in_bytes < 0) {
		return false;
	}
	struct beast_overlay *overlay = (struct beast_overlay *) buf_at(&buf2, 0);
	packet->type = type;
	uint64_t source_mlat = beast_parse_mlat(overlay->mlat_timestamp);
	packet->mlat_timestamp = packet_mlat_timestamp_scale_in(source_mlat, UINT64_C(0xffffffffffff), BEAST_MLAT_MHZ, &state->mlat_state);
	packet->rssi = packet_rssi_scale_in(overlay->rssi, UINT8_MAX);
	memcpy(packet->payload, buf_at(&buf2, sizeof(*overlay)), payload_bytes);
	buf_consume(buf, (size_t) in_bytes);
	return true;
}

static void beast_serialize_packet(struct packet *packet, struct buf *buf, uint8_t beast_type) {
	struct buf buf2 = BUF_INIT;
	size_t payload_bytes = packet_payload_len[packet->type];
	struct beast_overlay *overlay = (struct beast_overlay *) buf_at(&buf2, 0);
	overlay->one_a = 0x1a;
	overlay->type = beast_type;
	memcpy(buf_at(&buf2, sizeof(*overlay)), packet->payload, payload_bytes);
	beast_write_mlat(
			packet_mlat_timestamp_scale_out(packet->mlat_timestamp, UINT64_C(0xffffffffffff), BEAST_MLAT_MHZ),
			overlay->mlat_timestamp);

	if (packet->rssi) {
		overlay->rssi = (uint8_t) packet_rssi_scale_out(packet->rssi, UINT8_MAX);
	} else {
		overlay->rssi = UINT8_MAX;
	}

	buf2.length = sizeof(*overlay) + payload_bytes;
	beast_escape(buf, &buf2);
}

void beast_init() {
	assert(sizeof(struct beast_parser_state) <= PARSER_STATE_LEN);
	assert((sizeof(struct beast_overlay) + PACKET_PAYLOAD_LEN_MAX) * 2 <= BUF_LEN_MAX);
}

bool beast_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct beast_parser_state *state = (struct beast_parser_state *) state_in;
	struct beast_overlay *overlay = (struct beast_overlay *) buf_at(buf, 0);

	if (buf->length < sizeof(*overlay) ||
			buf_chr(buf, 0) != 0x1a) {
		return false;
	}

	switch (overlay->type) {
		case 0x31:
			return beast_parse_packet(buf, packet, state, PACKET_TYPE_MODE_AC);

		case 0x32:
			return beast_parse_packet(buf, packet, state, PACKET_TYPE_MODE_S_SHORT);

		case 0x33:
			return beast_parse_packet(buf, packet, state, PACKET_TYPE_MODE_S_LONG);

		default:
			return false;
	}
	return false;
}

void beast_serialize(struct packet *packet, struct buf *buf) {
	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_AC:
			beast_serialize_packet(packet, buf, 0x31);
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			beast_serialize_packet(packet, buf, 0x32);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			beast_serialize_packet(packet, buf, 0x33);
			break;
	}
}
