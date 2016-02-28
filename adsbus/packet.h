#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PACKET_DATA_LEN_MAX 14
struct packet {
	const uint8_t *source_id;
	enum packet_type {
		PACKET_TYPE_NONE,
		PACKET_TYPE_MODE_AC,
		PACKET_TYPE_MODE_S_SHORT,
		PACKET_TYPE_MODE_S_LONG,
	} type;
#define NUM_TYPES 4
	uint8_t payload[PACKET_DATA_LEN_MAX];
	uint64_t mlat_timestamp;
	uint32_t rssi;
};
extern char *packet_type_names[];
extern size_t packet_payload_len[];

#define PACKET_PAYLOAD_LEN_MAX 14

#define PACKET_MLAT_MHZ 120
// Use the signed max to avoid problems with some consumers; it's large enough to not matter.
#define PACKET_MLAT_MAX INT64_MAX
#define PACKET_RSSI_MAX UINT32_MAX

struct packet_mlat_state {
	uint64_t timestamp_last;
	uint64_t timestamp_generation;
};

uint64_t __attribute__ ((warn_unused_result)) packet_mlat_timestamp_scale_in(uint64_t, uint64_t, uint16_t, struct packet_mlat_state *);
uint64_t __attribute__ ((warn_unused_result)) packet_mlat_timestamp_scale_out(uint64_t, uint64_t, uint16_t);

uint32_t __attribute__ ((warn_unused_result)) packet_rssi_scale_in(uint32_t, uint32_t);
uint32_t __attribute__ ((warn_unused_result)) packet_rssi_scale_out(uint32_t, uint32_t);

void packet_sanity_check(const struct packet *);
bool __attribute__ ((warn_unused_result)) packet_validate_id(const uint8_t *);
