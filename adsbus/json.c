#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "hex.h"
#include "rand.h"
#include "receive.h"
#include "send.h"
#include "uuid.h"

#include "json.h"

static void json_serialize_to_buf(json_t *obj, struct buf *buf) {
	assert(json_dump_callback(obj, json_buf_append_callback, buf, 0) == 0);
	json_decref(obj);
	buf_chr(buf, buf->length++) = '\n';
}

static void json_hello(struct buf *buf) {
	json_t *hello = json_pack("{sIsIsI}",
			"mlat_timestamp_mhz", (json_int_t) MLAT_MHZ,
			"mlat_timestamp_max", (json_int_t) MLAT_MAX,
			"rssi_max", (json_int_t) RSSI_MAX);
	json_serialize_to_buf(hello, buf);
}

static void json_add_common(struct packet *packet, json_t *obj) {
	json_object_set_new(obj, "type", json_string(packet_type_names[packet->type]));
	if (packet->mlat_timestamp) {
		json_object_set_new(obj, "mlat_timestamp", json_integer(packet->mlat_timestamp));
	}
	if (packet->rssi) {
		json_object_set_new(obj, "rssi", json_integer(packet->rssi));
	}
}

static void json_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < MLAT_MAX);
	char hexbuf[14];
	hex_from_bin_upper(hexbuf, packet->payload, 7);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 14);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

static void json_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < MLAT_MAX);
	char hexbuf[28];
	hex_from_bin_upper(hexbuf, packet->payload, 14);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 28);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

void json_init() {
	assert(JSON_INTEGER_IS_LONG_LONG);

	size_t seed;
	rand_fill(&seed, sizeof(seed));
	json_object_seed(seed);
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
	}
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
