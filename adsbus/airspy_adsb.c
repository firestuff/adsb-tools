#include <assert.h>

#include "buf.h"
#include "hex.h"
#include "packet.h"
#include "receive.h"

#include "airspy_adsb.h"

#define SEND_MHZ 20

struct __attribute__((packed)) airspy_adsb_overlay {
	char semicolon1;
	uint8_t mlat_timestamp[8];
	char semicolon2;
	uint8_t mlat_precision[2];
	char semicolon3;
	uint8_t rssi[4];
	char semicolon4;
	char cr_lf;
	char lf;
};

struct airspy_adsb_parser_state {
	struct packet_mlat_state mlat_state;
};

static bool airspy_adsb_parse_packet(struct buf *buf, struct packet *packet, struct airspy_adsb_parser_state *state, enum packet_type type) {
	size_t payload_bytes = packet_payload_len[type];
	size_t overlay_start = 1 + (payload_bytes * 2);
	struct airspy_adsb_overlay *overlay = (struct airspy_adsb_overlay *) buf_at(buf, overlay_start);
	size_t total_len = overlay_start + sizeof(*overlay);

	if (((buf->length < total_len - 1 || overlay->cr_lf != '\n') &&
			 (buf->length < total_len || overlay->cr_lf != '\r' || overlay->lf != '\n')) ||
			buf_chr(buf, 0) != '*' ||
			overlay->semicolon1 != ';' ||
	    overlay->semicolon2 != ';' ||
			overlay->semicolon3 != ';' ||
			overlay->semicolon4 != ';') {
		return false;
	}
	uint16_t mlat_mhz = 2 * (uint16_t) hex_to_int(overlay->mlat_precision, sizeof(overlay->mlat_precision) / 2);
	if (!mlat_mhz) {
		return false;
	}
	int64_t mlat_timestamp_in = hex_to_int(overlay->mlat_timestamp, sizeof(overlay->mlat_timestamp) / 2);
	if (mlat_timestamp_in < 0) {
		return false;
	}
	packet->mlat_timestamp = packet_mlat_timestamp_scale_in((uint64_t) mlat_timestamp_in, UINT32_MAX, mlat_mhz, &state->mlat_state);
	packet->rssi = packet_rssi_scale_in((uint32_t) hex_to_int(overlay->rssi, sizeof(overlay->rssi) / 2), UINT16_MAX);
	packet->type = type;
	if (!hex_to_bin(packet->payload, buf_at(buf, 1), payload_bytes)) {
		return false;
	}
	buf_consume(buf, overlay->cr_lf == '\r' ? total_len : total_len - 1);
	return true;
}

void airspy_adsb_init() {
	assert(sizeof(struct airspy_adsb_parser_state) <= PARSER_STATE_LEN);
	assert(1 + PACKET_PAYLOAD_LEN_MAX + sizeof(struct airspy_adsb_overlay) < BUF_LEN_MAX);
}

bool airspy_adsb_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct airspy_adsb_parser_state *state = (struct airspy_adsb_parser_state *) state_in;

	return (
			airspy_adsb_parse_packet(buf, packet, state, PACKET_TYPE_MODE_AC) ||
			airspy_adsb_parse_packet(buf, packet, state, PACKET_TYPE_MODE_S_SHORT) ||
			airspy_adsb_parse_packet(buf, packet, state, PACKET_TYPE_MODE_S_LONG));
}

void airspy_adsb_serialize(struct packet *packet, struct buf *buf) {
	size_t payload_bytes = packet_payload_len[packet->type];
	size_t overlay_start = 1 + (payload_bytes * 2);
	struct airspy_adsb_overlay *overlay = (struct airspy_adsb_overlay *) buf_at(buf, overlay_start);
	size_t total_len = overlay_start + sizeof(*overlay);
	buf_chr(buf, 0) = '*';
	overlay->semicolon1 = overlay->semicolon2 = overlay->semicolon3 = overlay->semicolon4 =';';
	overlay->cr_lf = '\r';
	overlay->lf = '\n';
	hex_from_bin_upper(buf_at(buf, 1), packet->payload, payload_bytes);
	hex_from_int_upper(
			overlay->mlat_timestamp,
			packet_mlat_timestamp_scale_out(packet->mlat_timestamp, UINT32_MAX, SEND_MHZ),
			sizeof(overlay->mlat_timestamp) / 2);
	hex_from_int_upper(overlay->mlat_precision, SEND_MHZ / 2, sizeof(overlay->mlat_precision) / 2);
	hex_from_int_upper(overlay->rssi, packet_rssi_scale_out(packet->rssi, UINT16_MAX), sizeof(overlay->rssi) / 2);
	buf->length = total_len;
}
