#pragma once

#include <stdint.h>
#include <stddef.h>

void hex_init(void);
void hex_to_bin(uint8_t *, const uint8_t *, size_t);
uint64_t hex_to_int(const uint8_t *, size_t);
void hex_from_bin_upper(uint8_t *, const uint8_t *, size_t);
void hex_from_bin_lower(uint8_t *, const uint8_t *, size_t);
void hex_from_int_upper(uint8_t *, uint64_t, size_t);
void hex_from_int_lower(uint8_t *, uint64_t, size_t);
