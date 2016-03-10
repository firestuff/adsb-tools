#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "flow.h"
#include "opts.h"
#include "receive.h"
#include "send.h"

#include "stdinout.h"

static opts_group stdinout_opts;

static void stdinout_reopen(int fd, char *path, int flags) {
	// Presumes that all fds < fd are open
	assert(!close(fd));
	assert(open(path, flags | O_CLOEXEC | O_NOCTTY) == fd);
}

static bool stdinout_stdin(const char __attribute__((unused)) *arg) {
	int fd = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, receive_flow, NULL, NULL);
}

static bool stdinout_stdout(const char *arg) {
	struct serializer *serializer = send_get_serializer(arg);
	if (!serializer) {
		return false;
	}
	int fd = fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);
	assert(fd >= 0);
	return flow_new_send_hello(fd, send_flow, serializer, NULL);
}

void stdinout_opts_add() {
	opts_add("stdin", NULL, stdinout_stdin, stdinout_opts);
	opts_add("stdout", "FORMAT", stdinout_stdout, stdinout_opts);
}

void stdinout_init() {
	opts_call(stdinout_opts);
	stdinout_reopen(STDIN_FILENO, "/dev/null", O_RDONLY);
	stdinout_reopen(STDOUT_FILENO, "/dev/full", O_WRONLY);
}

void stdinout_cleanup() {
	assert(!close(STDIN_FILENO));
	assert(!close(STDOUT_FILENO));
}
