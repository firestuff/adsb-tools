#include <assert.h>
#include <stdio.h>

#include "client.h"
#include "json.h"


// Hobo JSON to avoid overhead. Assumes that we can't get quotes in the data.

void json_init() {
}

static size_t json_hello(char *buf) {
	int len = snprintf(buf, SERIALIZE_LEN,
	                   "{\"mlat_timestamp_mhz\":%ju,\"mlat_timestamp_max\":%ju,\"rssi_max\":%ju}\n",
					       		 (uintmax_t) MLAT_MHZ,
							       (uintmax_t) MLAT_MAX,
						         (uintmax_t) RSSI_MAX);
	assert(len < SERIALIZE_LEN);
	return len;
}

static size_t json_serialize_mode_s_short(struct packet *packet, char *buf) {
	char hexbuf[14];
	hex_from_bin(hexbuf, packet->data, 7);
	int len = snprintf(buf, SERIALIZE_LEN,
	                   "{\"payload\":\"%.14s\",\"mlat_timestamp\":%ju,\"rssi\":%ju}\n",
					           hexbuf,
					           (uintmax_t) packet->mlat_timestamp,
					           (uintmax_t) packet->rssi);
	assert(len < SERIALIZE_LEN);
	return len;
}

static size_t json_serialize_mode_s_long(struct packet *packet, char *buf) {
	char hexbuf[28];
	hex_from_bin(hexbuf, packet->data, 14);
	int len = snprintf(buf, SERIALIZE_LEN,
	                   "{\"payload\":\"%.28s\",\"mlat_timestamp\":%ju,\"rssi\":%ju}\n",
					           hexbuf,
					           (uintmax_t) packet->mlat_timestamp,
					           (uintmax_t) packet->rssi);
	assert(len < SERIALIZE_LEN);
	return len;
}

size_t json_serialize(struct packet *packet, char *buf) {
	if (!packet) {
		return json_hello(buf);
	}

	switch (packet->type) {
		case MODE_AC:
			return 0;

		case MODE_S_SHORT:
			return json_serialize_mode_s_short(packet, buf);

		case MODE_S_LONG:
			return json_serialize_mode_s_long(packet, buf);
	}
	
	return 0;
}
