#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "hex.h"
#include "buf.h"
#include "packet.h"
#include "rand.h"
#include "receive.h"
#include "send.h"
#include "server.h"
#include "uuid.h"

#include "json.h"

#define JSON_MAGIC "aDsB"

struct json_parser_state {
	struct packet_mlat_state mlat_state;
	uint16_t mlat_timestamp_mhz;
	uint64_t mlat_timestamp_max;
	uint32_t rssi_max;
	bool have_header;
};

static json_t *json_prev = NULL;

static void json_serialize_to_buf(json_t *obj, struct buf *buf) {
	assert(json_dump_callback(obj, json_buf_append_callback, buf, 0) == 0);
	json_decref(obj);
	buf_chr(buf, buf->length++) = '\n';
}

static void json_hello(struct buf *buf) {
	json_t *hello = json_pack(
			"{s:s, s:s, s:s, s:s, s:I, s:I, s:I}",
			"type", "header",
			"magic", JSON_MAGIC,
			"server_version", server_version,
			"server_id", server_id,
			"mlat_timestamp_mhz", (json_int_t) PACKET_MLAT_MHZ,
			"mlat_timestamp_max", (json_int_t) PACKET_MLAT_MAX,
			"rssi_max", (json_int_t) PACKET_RSSI_MAX);
	json_serialize_to_buf(hello, buf);
}

static void json_add_common(struct packet *packet, json_t *obj) {
	json_object_set_new(obj, "type", json_string(packet_type_names[packet->type]));
	json_object_set_new(obj, "source_id", json_string((const char *) packet->source_id));
	if (packet->mlat_timestamp) {
		json_object_set_new(obj, "mlat_timestamp", json_integer(packet->mlat_timestamp % INT64_MAX));
	}
	if (packet->rssi) {
		json_object_set_new(obj, "rssi", json_integer(packet->rssi));
	}
}

static void json_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	uint8_t hexbuf[14];
	hex_from_bin_upper(hexbuf, packet->payload, 7);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 14);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

static void json_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	uint8_t hexbuf[28];
	hex_from_bin_upper(hexbuf, packet->payload, 14);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 28);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

static bool json_parse_header(json_t *in, struct packet *packet, struct json_parser_state *state) {
	const char *magic, *json_server_id;
	json_int_t mlat_timestamp_mhz, mlat_timestamp_max, rssi_max;
	if (json_unpack(
			in, "{s:s, s:s, s:I, s:I, s:I}",
			"magic", &magic,
			"server_id", &json_server_id,
			"mlat_timestamp_mhz", &mlat_timestamp_mhz,
			"mlat_timestamp_max", &mlat_timestamp_max,
			"rssi_max", &rssi_max)) {
		return false;
	}

	if (strcmp(magic, JSON_MAGIC)) {
		return false;
	}

	if (mlat_timestamp_mhz > UINT16_MAX ||
			mlat_timestamp_max < 0 ||
			rssi_max > UINT32_MAX) {
		return false;
	}

	state->mlat_timestamp_mhz = (uint16_t) mlat_timestamp_mhz;
	state->mlat_timestamp_max = (uint64_t) mlat_timestamp_max;
	state->rssi_max = (uint32_t) rssi_max;

	if (!strcmp(json_server_id, (const char *) server_id)) {
		fprintf(stderr, "R %s: Attempt to receive json data from our own server ID (%s); loop!\n", packet->source_id, server_id);
		return false;
	}

	fprintf(stderr, "R %s: Connected to server ID: %s\n", packet->source_id, json_server_id);

	state->have_header = true;
	packet->type = PACKET_TYPE_NONE;
	return true;
}

static bool json_parse_common(json_t *in, struct packet *packet, struct json_parser_state *state) {
	if (!state->have_header) {
		return false;
	}

	if (json_unpack(in, "{s:s}", "source_id", &packet->source_id)) {
		return false;
	}

	if (!packet_validate_id(packet->source_id)) {
		return false;
	}

	json_t *mlat_timestamp = json_object_get(in, "mlat_timestamp");
	if (mlat_timestamp && json_is_integer(mlat_timestamp)) {
		json_int_t val = json_integer_value(mlat_timestamp);
		if (val < 0) {
			return false;
		}
		packet->mlat_timestamp = packet_mlat_timestamp_scale_in(
				(uint64_t) val,
				state->mlat_timestamp_max,
				state->mlat_timestamp_mhz,
				&state->mlat_state);
	}

	json_t *rssi = json_object_get(in, "rssi");
	if (rssi && json_is_integer(rssi)) {
		json_int_t val = json_integer_value(rssi);
		if (val > state->rssi_max) {
			return false;
		}
		packet->rssi = packet_rssi_scale_in((uint32_t) val, state->rssi_max);
	}

	return true;
}

static bool json_parse_mode_s_short(json_t *in, struct packet *packet, struct json_parser_state *state) {
	if (!json_parse_common(in, packet, state)) {
		return false;
	}

	json_t *payload = json_object_get(in, "payload");
	if (!payload || !json_is_string(payload) || json_string_length(payload) != 14) {
		return false;
	}

	if (!hex_to_bin(packet->payload, (const uint8_t *) json_string_value(payload), 7)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_SHORT;
	return true;
}

static bool json_parse_mode_s_long(json_t *in, struct packet *packet, struct json_parser_state *state) {
	if (!json_parse_common(in, packet, state)) {
		return false;
	}

	json_t *payload = json_object_get(in, "payload");
	if (!payload || !json_is_string(payload) || json_string_length(payload) != 28) {
		return false;
	}

	if (!hex_to_bin(packet->payload, (const uint8_t *) json_string_value(payload), 14)) {
		return false;
	}
	packet->type = PACKET_TYPE_MODE_S_LONG;
	return true;
}

void json_init() {
	assert(sizeof(struct json_parser_state) <= PARSER_STATE_LEN);
	assert(JSON_INTEGER_IS_LONG_LONG);

	size_t seed;
	rand_fill(&seed, sizeof(seed));
	json_object_seed(seed);
}

void json_cleanup() {
	if (json_prev) {
		json_decref(json_prev);
	}
}

bool json_parse(struct buf *buf, struct packet *packet, void *state_in) {
	struct json_parser_state *state = (struct json_parser_state *) state_in;

	if (json_prev) {
		json_decref(json_prev);
		json_prev = NULL;
	}

	json_error_t err;
	json_t *in = json_loadb((const char *) buf_at(buf, 0), buf->length, JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES, &err);
	if (!in) {
		return false;
	}
	if (!json_is_object(in)) {
		json_decref(in);
		return false;
	}

	json_t *type = json_object_get(in, "type");
	if (!type || !json_is_string(type)) {
		json_decref(in);
		return false;
	}
	const char *type_str = json_string_value(type);
	bool (*parser)(json_t *, struct packet *, struct json_parser_state *) = NULL;
	if (!strcmp(type_str, "header")) {
		parser = json_parse_header;
	} else if (!strcmp(type_str, "Mode-S short")) {
		parser = json_parse_mode_s_short;
	} else if (!strcmp(type_str, "Mode-S long")) {
		parser = json_parse_mode_s_long;
	}

	if (!parser || !parser(in, packet, state)) {
		json_decref(in);
		return false;
	}

	assert(err.position > 0);
	buf_consume(buf, (size_t) err.position);
	while (buf->length && (buf_chr(buf, 0) == '\r' || buf_chr(buf, 0) == '\n')) {
		buf_consume(buf, 1);
	}
	json_prev = in;
	return true;
}

void json_serialize(struct packet *packet, struct buf *buf) {
	if (!packet) {
		json_hello(buf);
		return;
	}

	switch (packet->type) {
		case PACKET_TYPE_NONE:
			break;

		case PACKET_TYPE_MODE_S_SHORT:
			json_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
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
