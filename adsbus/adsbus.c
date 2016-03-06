#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "wakeup.h"

static void print_usage(const char *name) {
	fprintf(stderr,
			"\n"
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			"\t--help\n"
			"\t--connect-receive=HOST/PORT\n"
			"\t--connect-send=FORMAT=HOST/PORT\n"
			"\t--connect-send-receive=FORMAT=HOST/PORT\n"
			"\t--listen-receive=[HOST/]PORT\n"
			"\t--listen-send=FORMAT=[HOST/]PORT\n"
			"\t--listen-send-receive=FORMAT=[HOST/]PORT\n"
			"\t--file-read=PATH\n"
			"\t--file-write=FORMAT=PATH\n"
			"\t--file-write-read=FORMAT=PATH\n"
			"\t--file-append=FORMAT=PATH\n"
			"\t--file-append-read=FORMAT=PATH\n"
			"\t--exec-receive=COMMAND\n"
			"\t--exec-send=FORMAT=COMMAND\n"
			"\t--exec-send-receive=FORMAT=COMMAND\n"
			"\t--stdin\n"
			"\t--stdout=FORMAT\n"
			, name);
	receive_print_usage();
	send_print_usage();
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"connect-receive",      required_argument, 0, 'c'},
		{"connect-send",         required_argument, 0, 's'},
		{"connect-send-receive", required_argument, 0, 't'},
		{"listen-receive",       required_argument, 0, 'l'},
		{"listen-send",          required_argument, 0, 'm'},
		{"listen-send-receive",  required_argument, 0, 'n'},
		{"file-read",            required_argument, 0, 'r'},
		{"file-write",           required_argument, 0, 'w'},
		{"file-write-read",      required_argument, 0, 'x'},
		{"file-append",          required_argument, 0, 'a'},
		{"file-append-read",     required_argument, 0, 'b'},
		{"exec-receive",         required_argument, 0, 'e'},
		{"exec-send",            required_argument, 0, 'f'},
		{"exec-send-receive",    required_argument, 0, 'g'},
		{"stdin",                no_argument,       0, 'i'},
		{"stdout",               required_argument, 0, 'o'},
		{"help",                 no_argument,       0, 'h'},
		{0,                      0,                 0, 0  },
	};

	int opt;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		bool (*handler)(char *) = NULL;
		switch (opt) {
			case 'c':
				handler = opts_add_connect_receive;
				break;

			case 's':
				handler = opts_add_connect_send;
				break;

			case 't':
				handler = opts_add_connect_send_receive;
				break;

			case 'l':
				handler = opts_add_listen_receive;
				break;

			case 'm':
				handler = opts_add_listen_send;
				break;

			case 'n':
				handler = opts_add_listen_send_receive;
				break;

			case 'r':
				handler = opts_add_file_read;
				break;

			case 'w':
				handler = opts_add_file_write;
				break;

			case 'x':
				handler = opts_add_file_write_read;
				break;

			case 'a':
				handler = opts_add_file_append;
				break;

			case 'b':
				handler = opts_add_file_append_read;
				break;

			case 'e':
				handler = opts_add_exec_receive;
				break;

			case 'f':
				handler = opts_add_exec_send;
				break;

			case 'g':
				handler = opts_add_exec_send_receive;
				break;

		  case 'i':
				handler = opts_add_stdin;
				break;

		  case 'o':
				handler = opts_add_stdout;
				break;

			case 'h':
			default:
				print_usage(argv[0]);
				return false;
		}

		if (handler) {
			if (!handler(optarg)) {
				fprintf(stderr, "Invalid flag value: %s\n", optarg);
				print_usage(argv[0]);
				return false;
			}
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Not a flag: %s\n", argv[optind]);
		print_usage(argv[0]);
		return false;
	}

	return true;
}

static void reopen(int fd, char *path, int flags) {
	// Presumes that all fds < fd are open
	assert(!close(fd));
	assert(open(path, flags | O_CLOEXEC) == fd);
}

int main(int argc, char *argv[]) {
	log_init();

	hex_init();
	rand_init();
	resolve_init();
	server_init();
	wakeup_init();
	peer_init();

	receive_init();
	send_init();

	beast_init();
	json_init();
	proto_init();
	stats_init();

	if (!parse_opts(argc, argv)) {
		peer_shutdown(0);
	}

	reopen(STDIN_FILENO, "/dev/null", O_RDONLY);
	reopen(STDOUT_FILENO, "/dev/full", O_WRONLY);

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

	peer_cleanup();

	log_cleanup();

	assert(!close(STDIN_FILENO));
	assert(!close(STDOUT_FILENO));
	close(STDERR_FILENO); // 2>&1 breaks this

	return EXIT_SUCCESS;
}
