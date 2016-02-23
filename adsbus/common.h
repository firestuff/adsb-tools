#pragma once

#include <stdint.h>


//////// packet

#define DATA_LEN_MAX 14
struct packet {
	enum {
		MODE_S_SHORT,
		MODE_S_LONG,
	} type;
	#define NUM_TYPES 2
	uint8_t payload[DATA_LEN_MAX];
	uint64_t mlat_timestamp;
	uint32_t rssi;
};
extern char *packet_type_names[];


//////// mlat

#define MLAT_MHZ 120
// Use the signed max to avoid problems with some consumers; it's large enough to not matter.
#define MLAT_MAX INT64_MAX
#define RSSI_MAX UINT32_MAX

struct mlat_state {
	uint64_t timestamp_last;
	uint64_t timestamp_generation;
};

uint64_t mlat_timestamp_scale_in(uint64_t, uint64_t, uint16_t, struct mlat_state *);
uint64_t mlat_timestamp_scale_out(uint64_t, uint64_t, uint16_t);


//////// rssi

uint32_t rssi_scale_in(uint32_t, uint32_t);
uint32_t rssi_scale_out(uint32_t, uint32_t);
