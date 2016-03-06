#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "log.h"

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static FILE *log_stream = NULL;

void log_init() {
	log_stream = fdopen(STDERR_FILENO, "a");
	assert(log_stream);
	setlinebuf(log_stream);
}

void log_cleanup() {
	assert(!fclose(log_stream));
}

__attribute__ ((__format__ (__printf__, 3, 4)))
void log_write(char type, const uint8_t *id, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	assert(fprintf(log_stream, "%c %s: ", type, id) > 0);
	assert(vfprintf(log_stream, fmt, ap) > 0);
	assert(fprintf(log_stream, "\n") == 1);
	va_end(ap);
}
