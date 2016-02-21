#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"
#include "wakeup.h"

#include "incoming.h"
#include "outgoing.h"

#include "receive.h"
#include "send.h"

#include "beast.h"
#include "json.h"
#include "stats.h"

#include "opts.h"

static void print_usage(const char *name) {
	fprintf(stderr,
			"\n"
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			"\t--help\n"
			"\t--dump=FORMAT\n"
			"\t--connect-receive=HOST/PORT\n"
			"\t--connect-send=FORMAT=HOST/PORT\n"
			"\t--listen-receive=[HOST/]PORT\n"
			"\t--listen-send=FORMAT=[HOST/]PORT\n"
			, name);
	receive_print_usage();
	send_print_usage();
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"dump",            required_argument, 0, 'd'},
		{"connect-receive", required_argument, 0, 'c'},
		{"connect-send",    required_argument, 0, 's'},
		{"listen-receive",  required_argument, 0, 'l'},
		{"listen-send",     required_argument, 0, 'm'},
		{"help",            no_argument,       0, 'h'},
		{0,                 0,                 0, 0  },
	};

	int opt;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		bool (*handler)(char *) = NULL;
		switch (opt) {
		  case 'd':
				handler = opts_add_dump;
				break;

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
	assert(!close(0));

	hex_init();

	peer_init();
	retry_init();
	wakeup_init();

	send_init();

	beast_init();
	json_init();
	stats_init();

	if (!parse_opts(argc, argv)) {
		return EXIT_FAILURE;
	}

	peer_loop();

	retry_cleanup();
	wakeup_cleanup();
	send_cleanup();

	incoming_cleanup();
	outgoing_cleanup();

	return EXIT_SUCCESS;
}
