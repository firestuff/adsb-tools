#pragma once

#include <stdint.h>

void log_init(void);
void log_cleanup(void);
void log_write(char, const uint8_t *, const char *, ...);
