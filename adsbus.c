#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include "common.h"
#include "backend.h"
#include "client.h"
#include "incoming.h"

#include "airspy_adsb.h"
#include "beast.h"
#include "json.h"
#include "stats.h"

static void print_usage(const char *name) {
	fprintf(stderr,
			"\n"
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			"\t--help\n"
			"\t--backend=HOST/PORT\n"
			"\t--dump=FORMAT\n"
			"\t--incoming=[HOST/]PORT\n"
			"\t--listen=FORMAT=[HOST/]PORT\n"
			, name);
	backend_print_usage();
	client_print_usage();
}

static bool add_dump(char *arg) {
	struct serializer *serializer = client_get_serializer(arg);
	if (!serializer) {
		fprintf(stderr, "Unknown --dump=FORMAT: %s\n", arg);
		return false;
	}
	client_add(1, serializer);
	return true;
}

static bool add_backend(char *arg) {
	char *port = strrchr(arg, '/');
	if (!port) {
		fprintf(stderr, "Invalid --backend=HOST/PORT (missing \"/\"): %s\n", arg);
		return false;
	}
	*(port++) = '\0';

	backend_new(arg, port);
	return true;
}

static bool add_incoming(char *arg){
	char *port = strrchr(arg, '/');
	if (port) {
		*(port++) = '\0';
		incoming_new(arg, port, backend_new_fd_wrapper, NULL);
	} else {
		incoming_new(NULL, arg, backend_new_fd_wrapper, NULL);
	}
	return true;
}

static bool add_listener(char *arg) {
	char *host_port = strchr(arg, '=');
	if (!host_port) {
		fprintf(stderr, "Invalid --listener=FORMAT=[HOST/]PORT (missing \"=\"): %s\n", arg);
		return false;
	}
	*(host_port++) = '\0';

	struct serializer *serializer = client_get_serializer(arg);
	if (!serializer) {
		fprintf(stderr, "Unknown --listener=FORMAT=[HOST/]PORT format: %s\n", arg);
		return false;
	}

	char *port = strrchr(host_port, '/');
	if (port) {
		*(port++) = '\0';
		incoming_new(host_port, port, client_add_wrapper, serializer);
	} else {
		incoming_new(NULL, host_port, client_add_wrapper, serializer);
	}
	return true;
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"backend",  required_argument, 0, 'b'},
		{"dump",     required_argument, 0, 'd'},
		{"incoming", required_argument, 0, 'i'},
		{"listen",   required_argument, 0, 'l'},
		{"help",     no_argument,       0, 'h'},
		{0,          0,                 0, 0  },
	};

	int opt;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		bool (*handler)(char *) = NULL;
		switch (opt) {
			case 'b':
				handler = add_backend;
				break;

		  case 'd':
				handler = add_dump;
				break;

			case 'h':
				print_usage(argv[0]);
				return false;

			case 'i':
				handler = add_incoming;
				break;

			case 'l':
				handler = add_listener;
				break;

			default:
				print_usage(argv[0]);
				return false;
		}

		if (handler) {
			if (!handler(optarg)) {
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
	peer_init();
	hex_init();
	client_init();
	airspy_adsb_init();
	beast_init();
	json_init();
	stats_init();

	if (!parse_opts(argc, argv)) {
		return EXIT_FAILURE;
	}

	peer_loop();
	return EXIT_SUCCESS;
}
