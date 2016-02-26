#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include "hex.h"

static uint8_t hex_table[256] = {0};
static uint8_t hex_upper_table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', };
static uint8_t hex_lower_table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', };

void hex_init() {
	for (uint8_t i = '0'; i <= '9'; i++) {
		hex_table[i] = i - '0';
	}
	for (uint8_t i = 'a'; i <= 'f'; i++) {
		hex_table[i] = 10 + i - 'a';
	}
	for (uint8_t i = 'A'; i <= 'F'; i++) {
		hex_table[i] = 10 + i - 'A';
	}
}

void hex_to_bin(uint8_t *out, const uint8_t *in, size_t bytes) {
	for (size_t i = 0, j = 0; i < bytes; i++, j += 2) {
		out[i] = (uint8_t) (hex_table[in[j]] << 4) | hex_table[in[j + 1]];
	}
}

uint64_t hex_to_int(const uint8_t *in, size_t bytes) {
	const uint8_t *in2 = (const uint8_t *) in;
	uint64_t ret = 0;
	bytes *= 2;
	for (size_t i = 0; i < bytes; i++) {
		ret <<= 4;
		ret |= hex_table[in2[i]];
	}
	return ret;
}

static void hex_from_bin(uint8_t *out, const uint8_t *in, size_t bytes, uint8_t table[]) {
	for (size_t i = 0, j = 0; i < bytes; i++, j += 2) {
		out[j] = table[in[i] >> 4];
		out[j + 1] = table[in[i] & 0xf];
	}
}

static void hex_from_int(uint8_t *out, uint64_t in, size_t bytes, uint8_t table[]) {
	bytes *= 2;
	assert(bytes < SSIZE_MAX);
	for (ssize_t o = (ssize_t) bytes - 1; o >= 0; o--) {
		out[o] = table[in & 0xf];
		in >>= 4;
	}
}

void hex_from_bin_upper(uint8_t *out, const uint8_t *in, size_t bytes) {
	hex_from_bin(out, in, bytes, hex_upper_table);
}

void hex_from_bin_lower(uint8_t *out, const uint8_t *in, size_t bytes) {
	hex_from_bin(out, in, bytes, hex_lower_table);
}

void hex_from_int_upper(uint8_t *out, uint64_t in, size_t bytes) {
	hex_from_int(out, in, bytes, hex_upper_table);
}

void hex_from_int_lower(uint8_t *out, uint64_t in, size_t bytes) {
	hex_from_int(out, in, bytes, hex_lower_table);
}
