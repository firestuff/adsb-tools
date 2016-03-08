#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LOG_STR(line) #line
#define LOG_LOC(file, line) (file ":" LOG_STR(line))
#define LOG(id, ...) log_write((log_module), LOG_LOC(__FILE__, __LINE__), (id), __VA_ARGS__)

void log_init(void);
void log_init2(void);
void log_cleanup(void);
bool log_reopen(const char *);
bool log_enable_timestamps(const char *);
void log_write(char, const char *, const uint8_t *, const char *, ...)
		__attribute__ ((__format__ (__printf__, 4, 5)));
