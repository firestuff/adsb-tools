#include <stdint.h>

#include "rand.h"
#include "common.h"

#include "uuid.h"

void uuid_gen(char *out) {
	uint8_t uuid[16];
	rand_fill(uuid, 16);
	uuid[6] = (uuid[6] & 0x0F) | 0x40;
	uuid[8] = (uuid[8] & 0x3F) | 0x80;

	out[8] = out[13] = out[18] = out[23] = '-';
	out[36] = '\0';
	hex_from_bin_lower(&out[0], &uuid[0], 4);
	hex_from_bin_lower(&out[9], &uuid[4], 2);
	hex_from_bin_lower(&out[14], &uuid[6], 2);
	hex_from_bin_lower(&out[19], &uuid[8], 2);
	hex_from_bin_lower(&out[24], &uuid[10], 6);
}
