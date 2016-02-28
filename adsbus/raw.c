#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buf.h"
#include "hex.h"
#include "packet.h"
#include "uuid.h"

#include "raw.h"

struct __attribute__((packed)) raw_mode_s_short_overlay {
	char asterisk;
	uint8_t payload[14];
	char semicolon;
	char cr_lf;
	char lf;
};

struct __attribute__((packed)) raw_mode_s_long_overlay {
	char asterisk;
	uint8_t payload[28];
	char semicolon;
	char cr_lf;
	char lf;
};

static bool raw_parse_mode_s_short(struct buf *buf, struct packet *packet) {
	struct raw_mode_s_short_overlay *overlay = (struct raw_mode_s_short_overlay *) buf_at(buf, 0);
	if (((buf->length < sizeof(*overlay) - 1 || overlay->cr_lf != '\n') &&
			 (buf->length < sizeof(*overlay) || overlay->cr_lf != '\r' || overlay->lf != '\n')) ||
			overlay->asterisk != '*' ||
			overlay->semicolon != ';') {
		return false;
	}
	if (!hex_to_bin(packet->payload, overlay->payload, sizeof(overlay->payload) / 2)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_SHORT;
	buf_consume(buf, overlay->cr_lf == '\r' ? sizeof(*overlay) : sizeof(*overlay) - 1);
	return true;
}

static bool raw_parse_mode_s_long(struct buf *buf, struct packet *packet) {
	struct raw_mode_s_long_overlay *overlay = (struct raw_mode_s_long_overlay *) buf_at(buf, 0);
	if (((buf->length < sizeof(*overlay) - 1 || overlay->cr_lf != '\n') &&
			 (buf->length < sizeof(*overlay) || overlay->cr_lf != '\r' || overlay->lf != '\n')) ||
			overlay->asterisk != '*' ||
			overlay->semicolon != ';') {
		return false;
	}
	if (!hex_to_bin(packet->payload, overlay->payload, sizeof(overlay->payload) / 2)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_LONG;
	buf_consume(buf, overlay->cr_lf == '\r' ? sizeof(*overlay) : sizeof(*overlay) - 1);
	return true;
}

static void raw_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	struct raw_mode_s_short_overlay *overlay = (struct raw_mode_s_short_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->lf = '\n';
	hex_from_bin_upper(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);
	buf->length = sizeof(*overlay);
}

static void raw_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	struct raw_mode_s_long_overlay *overlay = (struct raw_mode_s_long_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->lf = '\n';
	hex_from_bin_upper(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);
	buf->length = sizeof(*overlay);
}

void raw_init() {
	assert(sizeof(struct raw_mode_s_short_overlay) < BUF_LEN_MAX);
	assert(sizeof(struct raw_mode_s_long_overlay) < BUF_LEN_MAX);
}

bool raw_parse(struct buf *buf, struct packet *packet, void __attribute__((unused)) *state_in) {
	return (
			raw_parse_mode_s_short(buf, packet) ||
			raw_parse_mode_s_long(buf, packet));
}

void raw_serialize(struct packet *packet, struct buf *buf) {
	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			raw_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			raw_serialize_mode_s_long(packet, buf);
			break;
	}
}
