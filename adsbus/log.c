#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "uuid.h"

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static FILE *log_stream = NULL;
static char *log_path = NULL;
static uint8_t log_id[UUID_LEN];

static void log_rotate() {
	uint8_t old_log_id[UUID_LEN], new_log_id[UUID_LEN];
	uuid_gen(new_log_id);
	log_write('L', log_id, "Switching to new log with ID %s at: %s", new_log_id, log_path);
	memcpy(old_log_id, log_id, UUID_LEN);
	memcpy(log_id, new_log_id, UUID_LEN);
	assert(!fclose(log_stream));
	log_stream = fopen(log_path, "a");
	assert(log_stream);
	log_write('L', log_id, "Log start after switch from log ID %s", old_log_id);
}

void log_init() {
	int fd = dup(STDERR_FILENO);
	assert(fd >= 0);
	log_stream = fdopen(fd, "a");
	assert(log_stream);
	setlinebuf(log_stream);

	uuid_gen(log_id);
	log_write('L', log_id, "Log start");
}

void log_cleanup() {
	log_write('L', log_id, "Log end");
	assert(!fclose(log_stream));
	if (log_path) {
		free(log_path);
		log_path = NULL;
	}
}

bool log_reopen(const char *path) {
	if (log_path) {
		free(log_path);
	}
	log_path = strdup(path);
	assert(log_path);
	log_rotate();
	return true;
}

void log_write(char type, const uint8_t *id, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	assert(fprintf(log_stream, "%c %s: ", type, id) > 0);
	assert(vfprintf(log_stream, fmt, ap) > 0);
	assert(fprintf(log_stream, "\n") == 1);
	va_end(ap);
}
