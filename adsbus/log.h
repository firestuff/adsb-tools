#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LOG_STR(line) #line
#define LOG_LOC(file, line) (file ":" LOG_STR(line))
#define LOG(id, ...) log_write((log_module), LOG_LOC(__FILE__, __LINE__), (id), __VA_ARGS__)

void log_opts_add(void);
void log_init(void);
void log_cleanup(void);
void log_init_peer(void);
void log_cleanup_peer(void);
void log_write(char, const char *, const uint8_t *, const char *, ...)
		__attribute__ ((__format__ (__printf__, 4, 5)));
