#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"


#define MLAT_MHZ 120
#define RSSI_MAX UINT32_MAX


void buf_init(struct buf *buf) {
	buf->start = 0;
	buf->length = 0;
}

ssize_t buf_fill(struct buf *buf, int fd) {
	if (buf->start + buf->length == BUF_LEN_MAX) {
		assert(buf->start > 0);
		memmove(buf->buf, buf_at(buf, 0), buf->length);
		buf->start = 0;
	}

	size_t space = BUF_LEN_MAX - buf->length - buf->start;
	ssize_t in = read(fd, buf_at(buf, buf->length), space);
	if (in < 0) {
		return in;
	}
	buf->length += in;
	return in;
}

void buf_consume(struct buf *buf, size_t length) {
	assert(buf->length >= length);

	buf->length -= length;
	if (buf->length) {
		buf->start += length;
	} else {
		buf->start = 0;
	}
}


uint64_t mlat_timestamp_scale_mhz_in(uint64_t timestamp, uint32_t mhz) {
	return timestamp * (MLAT_MHZ / mhz);
}

uint64_t mlat_timestamp_scale_width_in(uint64_t timestamp, uint64_t max, struct mlat_state *state) {
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


uint32_t rssi_scale_in(uint32_t value, uint32_t max) {
	return value * (RSSI_MAX / max);
}


static uint8_t hex_table[256] = {0};

void hex_init() {
	for (int i = '0'; i <= '9'; i++) {
		hex_table[i] = i - '0';
	}
	for (int i = 'a'; i <= 'f'; i++) {
		hex_table[i] = 10 + i - 'a';
	}
	for (int i = 'A'; i <= 'F'; i++) {
		hex_table[i] = 10 + i - 'A';
	}
}

void hex_to_bin(char *out, char *in, size_t bytes) {
	uint8_t *in2 = (uint8_t *) in;
	for (size_t i = 0, j = 0; i < bytes; i++, j += 2) {
		out[i] = (hex_table[in2[j]] << 4) | hex_table[in2[j + 1]];
	}
}

uint64_t hex_to_int(char *in, size_t bytes) {
	uint8_t *in2 = (uint8_t *) in;
	uint64_t ret = 0;
	bytes *= 2;
	for (size_t i = 0; i < bytes; i++) {
		ret <<= 4;
		ret |= hex_table[in2[i]];
	}
	return ret;
}
