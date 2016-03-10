#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "opts.h"
#include "peer.h"
#include "uuid.h"

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static FILE *log_stream = NULL;
static char *log_path = NULL;
static uint8_t log_id[UUID_LEN];
static struct peer log_rotate_peer;
static bool log_timestamps = false;
static bool log_quiet = false;
static opts_group log_opts;

static char log_module = 'L';

static void log_open() {
	if (!log_path) {
		return;
	}
	int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND | O_NOCTTY | O_CLOEXEC, S_IRUSR | S_IWUSR);
	assert(fd >= 0);
	if (fd != STDERR_FILENO) {
		assert(dup3(fd, STDERR_FILENO, O_CLOEXEC) == STDERR_FILENO);
		assert(!close(fd));
	}
}

static void log_rotate() {
	assert(log_path);

	uint8_t old_log_id[UUID_LEN], new_log_id[UUID_LEN];
	uuid_gen(new_log_id);
	LOG(log_id, "Switching to new log with ID %s at: %s", new_log_id, log_path);
	memcpy(old_log_id, log_id, UUID_LEN);
	memcpy(log_id, new_log_id, UUID_LEN);
	log_open();
	LOG(log_id, "Log start after switch from log ID %s", old_log_id);
}

static void log_rotate_handler(struct peer *peer) {
	struct signalfd_siginfo siginfo;
	assert(read(peer->fd, &siginfo, sizeof(siginfo)) == sizeof(siginfo));
	LOG(log_id, "Received signal %u; rotating logs", siginfo.ssi_signo);
	log_rotate();
}

static bool log_set_path(const char *path) {
	if (log_path) {
		return false;
	}
	log_path = strdup(path);
	assert(log_path);
	return true;
}

static bool log_enable_timestamps(const char __attribute__ ((unused)) *arg) {
	log_timestamps = true;
	return true;
}

static bool log_set_quiet(const char __attribute__ ((unused)) *arg) {
	log_quiet = true;
	return true;
}

void log_opts_add() {
	opts_add("log-file", "PATH", log_set_path, log_opts);
	opts_add("log-timestamps", NULL, log_enable_timestamps, log_opts);
	opts_add("quiet", NULL, log_set_quiet, log_opts);
}

void log_init() {
	opts_call(log_opts);
	log_open();
	log_stream = fdopen(STDERR_FILENO, "a");
	assert(log_stream);
	setlinebuf(log_stream);

	uuid_gen(log_id);
	LOG(log_id, "Log start");
}

void log_init_peer() {
	if (log_path) {
		sigset_t sigmask;
		assert(!sigemptyset(&sigmask));
		assert(!sigaddset(&sigmask, SIGHUP));
		log_rotate_peer.fd = signalfd(-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
		assert(log_rotate_peer.fd >= 0);
		log_rotate_peer.event_handler = log_rotate_handler;
		peer_epoll_add(&log_rotate_peer, EPOLLIN);

		assert(!sigprocmask(SIG_BLOCK, &sigmask, NULL));
	} else {
		log_rotate_peer.fd = -1;
	}
}

void log_cleanup_peer() {
	peer_close(&log_rotate_peer);
}

void log_cleanup() {
	LOG(log_id, "Log end");
	assert(!fclose(log_stream));
	if (log_path) {
		free(log_path);
		log_path = NULL;
	}
}

void log_write(char type, const char *loc, const uint8_t *id, const char *fmt, ...) {
	if (log_quiet) {
		return;
	}

	va_list ap;
	va_start(ap, fmt);

	char datetime[64] = "";
	if (log_timestamps) {
		time_t now;
		assert(time(&now) >= 0);
		struct tm tmnow;
		assert(gmtime_r(&now, &tmnow));
		assert(strftime(datetime, sizeof(datetime), "%FT%TZ ", &tmnow) > 0);
	}

	assert(fprintf(log_stream, "%s[%18s] %c %s: ", datetime, loc, type, id) > 0);
	assert(vfprintf(log_stream, fmt, ap) > 0);
	assert(fprintf(log_stream, "\n") == 1);
	va_end(ap);
}
