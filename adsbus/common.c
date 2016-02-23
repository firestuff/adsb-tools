#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "common.h"
#include "uuid.h"
#include "wakeup.h"


char *packet_type_names[] = {
	"Mode-S short",
	"Mode-S long",
};


static uint64_t mlat_timestamp_scale_mhz_in(uint64_t timestamp, uint32_t mhz) {
	return timestamp * (MLAT_MHZ / mhz);
}

static uint64_t mlat_timestamp_scale_width_in(uint64_t timestamp, uint64_t max, struct mlat_state *state) {
	if (timestamp < state->timestamp_last) {
		// Counter reset
		state->timestamp_generation += max;
	}

	state->timestamp_last = timestamp;
	return state->timestamp_generation + timestamp;
}

uint64_t mlat_timestamp_scale_in(uint64_t timestamp, uint64_t max, uint16_t mhz, struct mlat_state *state) {
	return mlat_timestamp_scale_mhz_in(mlat_timestamp_scale_width_in(timestamp, max, state), mhz);
}

static uint64_t mlat_timestamp_scale_mhz_out(uint64_t timestamp, uint64_t mhz) {
	return timestamp / (MLAT_MHZ / mhz);
}

static uint64_t mlat_timestamp_scale_width_out(uint64_t timestamp, uint64_t max) {
	return timestamp % max;
}

uint64_t mlat_timestamp_scale_out(uint64_t timestamp, uint64_t max, uint16_t mhz) {
	return mlat_timestamp_scale_width_out(mlat_timestamp_scale_mhz_out(timestamp, mhz), max);
}


uint32_t rssi_scale_in(uint32_t value, uint32_t max) {
	return value * (RSSI_MAX / max);
}

uint32_t rssi_scale_out(uint32_t value, uint32_t max) {
	return value / (RSSI_MAX / max);
}
