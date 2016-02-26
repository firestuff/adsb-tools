#include <assert.h>

#include "packet.h"

char *packet_type_names[] = {
	"INVALID",
	"Mode-S short",
	"Mode-S long",
};

static uint64_t packet_mlat_timestamp_scale_mhz_in(uint64_t timestamp, uint32_t mhz) {
	return timestamp * (PACKET_MLAT_MHZ / mhz);
}

static uint64_t packet_mlat_timestamp_scale_width_in(uint64_t timestamp, uint64_t max, struct packet_mlat_state *state) {
	if (timestamp < state->timestamp_last) {
		// Counter reset
		state->timestamp_generation += max;
	}

	state->timestamp_last = timestamp;
	return state->timestamp_generation + timestamp;
}

static uint64_t packet_mlat_timestamp_scale_mhz_out(uint64_t timestamp, uint64_t mhz) {
	return timestamp / (PACKET_MLAT_MHZ / mhz);
}

static uint64_t packet_mlat_timestamp_scale_width_out(uint64_t timestamp, uint64_t max) {
	return timestamp % max;
}

uint64_t packet_mlat_timestamp_scale_in(uint64_t timestamp, uint64_t max, uint16_t mhz, struct packet_mlat_state *state) {
	return packet_mlat_timestamp_scale_mhz_in(packet_mlat_timestamp_scale_width_in(timestamp, max, state), mhz) % PACKET_MLAT_MAX;
}

uint64_t packet_mlat_timestamp_scale_out(uint64_t timestamp, uint64_t max, uint16_t mhz) {
	return packet_mlat_timestamp_scale_width_out(packet_mlat_timestamp_scale_mhz_out(timestamp, mhz), max);
}

uint32_t packet_rssi_scale_in(uint32_t value, uint32_t max) {
	return value * (PACKET_RSSI_MAX / max);
}

uint32_t packet_rssi_scale_out(uint32_t value, uint32_t max) {
	return value / (PACKET_RSSI_MAX / max);
}

void packet_sanity_check(const struct packet *packet) {
	assert(packet->source_id);
	assert(packet->type > PACKET_TYPE_NONE && packet->type < NUM_TYPES);
	assert(packet->mlat_timestamp <= PACKET_MLAT_MAX);
	assert(packet->rssi <= PACKET_RSSI_MAX);
}
