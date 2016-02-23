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
	json_t *hello = json_pack("{sssssssIsIsI}",
			"type", "header",
			"server_version", server_version,
			"server_id", server_id,
			"mlat_timestamp_mhz", (json_int_t) PACKET_MLAT_MHZ,
			"mlat_timestamp_max", (json_int_t) PACKET_MLAT_MAX,
			"rssi_max", (json_int_t) PACKET_RSSI_MAX);
	json_serialize_to_buf(hello, buf);
}

static void json_add_common(struct packet *packet, json_t *obj) {
	json_object_set_new(obj, "type", json_string(packet_type_names[packet->type]));
	json_object_set_new(obj, "source_id", json_string(packet->source_id));
	if (packet->mlat_timestamp) {
		json_object_set_new(obj, "mlat_timestamp", json_integer(packet->mlat_timestamp));
	}
	if (packet->rssi) {
		json_object_set_new(obj, "rssi", json_integer(packet->rssi));
	}
}

static void json_serialize_mode_s_short(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < PACKET_MLAT_MAX);
	char hexbuf[14];
	hex_from_bin_upper(hexbuf, packet->payload, 7);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 14);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

static void json_serialize_mode_s_long(struct packet *packet, struct buf *buf) {
	assert(packet->mlat_timestamp < PACKET_MLAT_MAX);
	char hexbuf[28];
	hex_from_bin_upper(hexbuf, packet->payload, 14);
	json_t *out = json_pack("{ss#}", "payload", hexbuf, 28);
	json_add_common(packet, out);
	json_serialize_to_buf(out, buf);
}

static bool json_parse_common(json_t *in, struct packet *packet, struct json_parser_state *state) {
	if (!state->have_header) {
		return false;
	}

	json_t *source_id = json_object_get(in, "source_id");
	if (!source_id || !json_is_string(source_id)) {
		return false;
	}
	packet->source_id = json_string_value(source_id);

	json_t *mlat_timestamp = json_object_get(in, "mlat_timestamp");
	if (mlat_timestamp && json_is_integer(mlat_timestamp)) {
		packet->mlat_timestamp = packet_mlat_timestamp_scale_in(
				json_integer_value(mlat_timestamp),
				state->mlat_timestamp_max,
				state->mlat_timestamp_mhz,
				&state->mlat_state);
	}

	json_t *rssi = json_object_get(in, "rssi");
	if (rssi && json_is_integer(rssi)) {
		packet->rssi = packet_rssi_scale_in(json_integer_value(rssi), state->rssi_max);
	}

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
	json_t *in = json_loadb(buf_at(buf, 0), buf->length, JSON_DISABLE_EOF_CHECK | JSON_REJECT_DUPLICATES, &err);
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
	if (!strcmp(type_str, "header")) {
		json_t *mlat_timestamp_mhz = json_object_get(in, "mlat_timestamp_mhz");
		if (!mlat_timestamp_mhz || !json_is_integer(mlat_timestamp_mhz)) {
			json_decref(in);
			return false;
		}
		state->mlat_timestamp_mhz = json_integer_value(mlat_timestamp_mhz);

		json_t *mlat_timestamp_max = json_object_get(in, "mlat_timestamp_max");
		if (!mlat_timestamp_max || !json_is_integer(mlat_timestamp_max)) {
			json_decref(in);
			return false;
		}
		state->mlat_timestamp_max = json_integer_value(mlat_timestamp_max);

		json_t *rssi_max = json_object_get(in, "rssi_max");
		if (!rssi_max || !json_is_integer(rssi_max)) {
			json_decref(in);
			return false;
		}
		state->rssi_max = json_integer_value(rssi_max);

		json_t *json_server_id = json_object_get(in, "server_id");
		if (!json_server_id || !json_is_string(json_server_id)) {
			json_decref(in);
			return false;
		}
		const char *server_id_str = json_string_value(json_server_id);
		if (!strcmp(server_id_str, server_id)) {
			fprintf(stderr, "R %s: Attempt to receive json data from our own server ID (%s); loop!\n", packet->source_id, server_id);
			json_decref(in);
			return false;
		}

		fprintf(stderr, "R %s: Connected to server ID: %s\n", packet->source_id, server_id_str);

		state->have_header = true;
		packet->type = PACKET_TYPE_NONE;
	} else if (!strcmp(type_str, "Mode-S short")) {
		if (!json_parse_common(in, packet, state)) {
			json_decref(in);
			return false;
		}

		json_t *payload = json_object_get(in, "payload");
		if (!payload || !json_is_string(payload) || json_string_length(payload) != 14) {
			json_decref(in);
			return false;
		}

		hex_to_bin(packet->payload, json_string_value(payload), 7);
		packet->type = PACKET_TYPE_MODE_S_SHORT;
	} else if (!strcmp(type_str, "Mode-S long")) {
		if (!json_parse_common(in, packet, state)) {
			json_decref(in);
			return false;
		}

		json_t *payload = json_object_get(in, "payload");
		if (!payload || !json_is_string(payload) || json_string_length(payload) != 28) {
			json_decref(in);
			return false;
		}

		hex_to_bin(packet->payload, json_string_value(payload), 14);
		packet->type = PACKET_TYPE_MODE_S_LONG;
	} else {
		json_decref(in);
		return false;
	}

	buf_consume(buf, err.position);
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
		case PACKET_TYPE_MODE_S_SHORT:
			json_serialize_mode_s_short(packet, buf);
			break;

		case PACKET_TYPE_MODE_S_LONG:
			json_serialize_mode_s_long(packet, buf);
			break;

		default:
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
