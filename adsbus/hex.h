#pragma once

#include <stdint.h>
#include <stddef.h>

void hex_init();
void hex_to_bin(uint8_t *, const char *, size_t);
uint64_t hex_to_int(const char *, size_t);
void hex_from_bin_upper(char *, const uint8_t *, size_t);
void hex_from_bin_lower(char *, const uint8_t *, size_t);
void hex_from_int_upper(char *, uint64_t, size_t);
void hex_from_int_lower(char *, uint64_t, size_t);
