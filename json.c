#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "backend.h"
#include "client.h"
#include "json.h"


// Hobo JSON to avoid overhead. Assumes that we can't get quotes in the data.

void json_init() {
	assert(JSON_INTEGER_IS_LONG_LONG);
}

int json_buf_append_callback(const char *buffer, size_t size, void *data) {
	struct buf *buf = data;
	if (buf->length + size + 1 > BUF_LEN_MAX) {
		return -1;
	}
	memcpy(buf_at(buf, buf->length), buffer, size);
	buf->length += size;
	return 0;
}

static void json_hello(struct buf *buf) {
	json_t *hello = json_pack("{sssIsIsI}",
			"server_id", server_id,
			"mlat_timestamp_mhz", (json_int_t) MLAT_MHZ,
			"mlat_timestamp_max", (json_int_t) MLAT_MAX,
			"rssi_max", (json_int_t) RSSI_MAX);
	assert(json_dump_callback(hello, json_buf_append_callback, buf, 0) == 0);
	json_decref(hello);
	buf_chr(buf, buf->length++) = '\n';
}

static void json_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < MLAT_MAX);
	char hexbuf[14];
	hex_from_bin(hexbuf, packet->payload, 7);
	json_t *out = json_pack("{ssss#sIsI}",
			"backend_id", packet->backend->id,
			"payload", hexbuf, 14,
			"mlat_timestamp", (json_int_t) packet->mlat_timestamp,
			"rssi", (json_int_t) packet->rssi);
	assert(json_dump_callback(out, json_buf_append_callback, buf, 0) == 0);
	json_decref(out);
	buf_chr(buf, buf->length++) = '\n';
}

static void json_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < MLAT_MAX);
	char hexbuf[28];
	hex_from_bin(hexbuf, packet->payload, 14);
	json_t *out = json_pack("{ssss#sIsI}",
			"backend_id", packet->backend->id,
			"payload", hexbuf, 28,
			"mlat_timestamp", (json_int_t) packet->mlat_timestamp,
			"rssi", (json_int_t) packet->rssi);
	assert(json_dump_callback(out, json_buf_append_callback, buf, 0) == 0);
	json_decref(out);
	buf_chr(buf, buf->length++) = '\n';
}

void json_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		json_hello(buf);
		return;
	}

	switch (packet->type) {
		case MODE_S_SHORT:
			json_serialize_mode_s_short(packet, buf);
			break;

		case MODE_S_LONG:
			json_serialize_mode_s_long(packet, buf);
			break;

		case NUM_TYPES:
			break;
	}
}
