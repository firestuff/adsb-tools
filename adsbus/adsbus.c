#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "beast.h"
#include "exec.h"
#include "file.h"
#include "hex.h"
#include "incoming.h"
#include "json.h"
#include "log.h"
#include "opts.h"
#include "outgoing.h"
#include "peer.h"
#include "proto.h"
#include "rand.h"
#include "receive.h"
#include "resolve.h"
#include "send.h"
#include "send_receive.h"
#include "server.h"
#include "stats.h"
#include "stdinout.h"
#include "wakeup.h"

static void reopen(int fd, char *path, int flags) {
	// Presumes that all fds < fd are open
	assert(!close(fd));
	assert(open(path, flags | O_CLOEXEC | O_NOCTTY) == fd);
}

static void adsbus_opts_add() {
	// This order controls the order in --help, but nothing else.
	server_opts_add();
	log_opts_add();
	outgoing_opts_add();
	incoming_opts_add();
	exec_opts_add();
	file_opts_add();
	stdinout_opts_add();
}

int main(int argc, char *argv[]) {
	adsbus_opts_add();

	opts_init(argc, argv);

	hex_init();
	rand_init();

	log_init();
	server_init();

	resolve_init();
	wakeup_init();
	peer_init();

	log_init_peer();

	receive_init();
	send_init();

	beast_init();
	json_init();
	proto_init();
	stats_init();

	outgoing_init();
	incoming_init();
	exec_init();
	file_init();
	stdinout_init();

	reopen(STDIN_FILENO, "/dev/null", O_RDONLY);
	reopen(STDOUT_FILENO, "/dev/full", O_WRONLY);
	reopen(STDERR_FILENO, "/dev/full", O_WRONLY);

	peer_loop();

	resolve_cleanup();

	receive_cleanup();
	send_cleanup();
	send_receive_cleanup();
	incoming_cleanup();
	outgoing_cleanup();
	exec_cleanup();
	file_cleanup();

	json_cleanup();
	proto_cleanup();

	rand_cleanup();
	wakeup_cleanup();

	log_cleanup_peer();

	peer_cleanup();

	log_cleanup();

	assert(!close(STDIN_FILENO));
	assert(!close(STDOUT_FILENO));
	assert(!close(STDERR_FILENO));

	return EXIT_SUCCESS;
}
