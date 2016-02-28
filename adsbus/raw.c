#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "hex.h"
#include "packet.h"
#include "uuid.h"

#include "raw.h"

struct __attribute__((packed)) raw_overlay {
	char semicolon;
	char cr_lf;
	char lf;
};

static bool raw_parse_packet(struct buf *buf, struct packet *packet, enum packet_type type) {
	size_t payload_bytes = packet_payload_len[type];
	size_t overlay_start = 1 + payload_bytes;
	struct raw_overlay *overlay = (struct raw_overlay *) buf_at(buf, overlay_start);
	size_t total_len = overlay_start + sizeof(*overlay);

	if (((buf->length < sizeof(*overlay) - 1 || overlay->cr_lf != '\n') &&
			 (buf->length < sizeof(*overlay) || overlay->cr_lf != '\r' || overlay->lf != '\n')) ||
			buf_chr(buf, 0) != '*' ||
			overlay->semicolon != ';') {
		return false;
	}
	if (!hex_to_bin(packet->payload, buf_at(buf, 1), payload_bytes)) {
		return false;
	}
	packet->type = type;
	buf_consume(buf, overlay->cr_lf == '\r' ? total_len : total_len - 1);
	return true;
}

void raw_init() {
	assert(1 + PACKET_PAYLOAD_LEN_MAX + sizeof(struct raw_overlay) < BUF_LEN_MAX);
}

bool raw_parse(struct buf *buf, struct packet *packet, void __attribute__((unused)) *state_in) {
	return (
			raw_parse_packet(buf, packet, PACKET_TYPE_MODE_AC) ||
			raw_parse_packet(buf, packet, PACKET_TYPE_MODE_S_SHORT) ||
			raw_parse_packet(buf, packet, PACKET_TYPE_MODE_S_LONG));
}

void raw_serialize(struct packet *packet, struct buf *buf) {
	size_t payload_bytes = packet_payload_len[packet->type];
	struct raw_overlay *overlay = (struct raw_overlay *) buf_at(buf, 1 + payload_bytes);
	buf_chr(buf, 0) = '*';
	overlay->semicolon = ';';
	overlay->lf = '\n';
	hex_from_bin_upper(buf_at(buf, 1), packet->payload, payload_bytes);
	buf->length = 1 + payload_bytes + sizeof(*overlay);
}
