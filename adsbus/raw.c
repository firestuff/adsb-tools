#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "raw.h"

struct __attribute__((packed)) raw_mode_s_short_overlay {
	char asterisk;
	char payload[14];
	char semicolon;
	char lf;
};

struct __attribute__((packed)) raw_mode_s_long_overlay {
	char asterisk;
	char payload[28];
	char semicolon;
	char lf;
};

static bool raw_parse_mode_s_short(struct buf *buf, struct packet *packet) {
	struct raw_mode_s_short_overlay *short_overlay = (struct raw_mode_s_short_overlay *) buf_at(buf, 0);
	if (buf->length < sizeof(*short_overlay) ||
			short_overlay->asterisk != '*' ||
			short_overlay->semicolon != ';' ||
			short_overlay->lf != '\n') {
		return false;
	}
	packet->type = MODE_S_SHORT;
	hex_to_bin(packet->payload, short_overlay->payload, sizeof(short_overlay->payload) / 2);
	buf_consume(buf, sizeof(*short_overlay));
	return true;
}

static bool raw_parse_mode_s_long(struct buf *buf, struct packet *packet) {
	struct raw_mode_s_long_overlay *long_overlay = (struct raw_mode_s_long_overlay *) buf_at(buf, 0);
	if (buf->length < sizeof(*long_overlay) ||
			long_overlay->asterisk != '*' ||
			long_overlay->semicolon != ';' ||
			long_overlay->lf != '\n') {
		return false;
	}
	packet->type = MODE_S_LONG;
	hex_to_bin(packet->payload, long_overlay->payload, sizeof(long_overlay->payload) / 2);
	buf_consume(buf, sizeof(*long_overlay));
	return true;
}

static void raw_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	struct raw_mode_s_short_overlay *overlay = (struct raw_mode_s_short_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->lf = '\n';
	hex_from_bin(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);
	buf->length = sizeof(*overlay);
}

static void raw_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	struct raw_mode_s_long_overlay *overlay = (struct raw_mode_s_long_overlay *) buf_at(buf, 0);
	overlay->asterisk = '*';
	overlay->semicolon = ';';
	overlay->lf = '\n';
	hex_from_bin(overlay->payload, packet->payload, sizeof(overlay->payload) / 2);
	buf->length = sizeof(*overlay);
}

void raw_init() {
	assert(sizeof(struct raw_mode_s_short_overlay) < BUF_LEN_MAX);
	assert(sizeof(struct raw_mode_s_long_overlay) < BUF_LEN_MAX);
}

bool raw_parse(struct buf *buf, struct packet *packet, void *state_in) {
	return (
			raw_parse_mode_s_short(buf, packet) ||
			raw_parse_mode_s_long(buf, packet));
}

void raw_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		return;
	}

	switch (packet->type) {
		case MODE_S_SHORT:
			raw_serialize_mode_s_short(packet, buf);
			break;

		case MODE_S_LONG:
			raw_serialize_mode_s_long(packet, buf);
			break;
	}
}
