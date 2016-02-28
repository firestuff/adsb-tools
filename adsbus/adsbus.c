#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "beast.h"
#include "hex.h"
#include "incoming.h"
#include "json.h"
#include "opts.h"
#include "outgoing.h"
#include "peer.h"
#include "proto.h"
#include "rand.h"
#include "receive.h"
#include "resolve.h"
#include "send.h"
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
			"\t--listen-receive=[HOST/]PORT\n"
			"\t--listen-send=FORMAT=[HOST/]PORT\n"
			"\t--stdin\n"
			"\t--stdout=FORMAT\n"
			, name);
	receive_print_usage();
	send_print_usage();
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"connect-receive", required_argument, 0, 'c'},
		{"connect-send",    required_argument, 0, 's'},
		{"listen-receive",  required_argument, 0, 'l'},
		{"listen-send",     required_argument, 0, 'm'},
		{"stdin",           no_argument,       0, 'i'},
		{"stdout",          required_argument, 0, 'o'},
		{"help",            no_argument,       0, 'h'},
		{0,                 0,                 0, 0  },
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

			case 'l':
				handler = opts_add_listen_receive;
				break;

			case 'm':
				handler = opts_add_listen_send;
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

int main(int argc, char *argv[]) {
	hex_init();
	rand_init();
	resolve_init();
	server_init();
	wakeup_init();
	peer_init();

	send_init();

	beast_init();
	json_init();
	proto_init();
	stats_init();

	if (!parse_opts(argc, argv)) {
		peer_shutdown(0);
	}

	assert(!close(0));
	assert(!close(1));

	peer_loop();

	resolve_cleanup();

	receive_cleanup();
	send_cleanup();
	incoming_cleanup();
	outgoing_cleanup();

	json_cleanup();
	proto_cleanup();

	rand_cleanup();
	wakeup_cleanup();

	peer_cleanup();

	assert(!close(2));

	return EXIT_SUCCESS;
}
