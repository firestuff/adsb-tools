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


static bool add_dump(char *format) {
	struct serializer *serializer = client_get_serializer(format);
	if (!serializer) {
		fprintf(stderr, "Unknown dump format: %s\n", format);
		return false;
	}
	client_add(1, serializer);
	return true;
}

static void print_usage(char *argv[]) {
	fprintf(stderr,
			"Usage: %s [OPTION]...\n"
			"\n"
			"Options:\n"
			"\t--help\n"
			"\t--backend=HOST/PORT\n"
			"\t--dump=FORMAT\n"
			"\t--incoming=[HOST/]PORT\n"
			"\t--listen=FORMAT=[HOST/]PORT\n"
			, argv[0]);
}

static bool parse_opts(int argc, char *argv[]) {
	static struct option long_options[] = {
		{"backend",  required_argument, 0, 'b'},
		{"dump",     required_argument, 0, 'd'},
		{"incoming", required_argument, 0, 'i'},
		{"listen",   required_argument, 0, 'l'},
		{"help",     no_argument,       0, 'h'},
	};

	int opt;
	char *delim1, *delim2;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		switch (opt) {
			case 'b':
				// It would be really nice if libc had a standard way to split host:port.
				delim1 = strrchr(optarg, '/');
				if (delim1 == NULL) {
					print_usage(argv);
					return false;
				}
				*delim1 = '\0';
				delim1++;

				backend_new(optarg, delim1);
				break;

		  case 'd':
				if (!add_dump(optarg)) {
					return false;
				}
				break;

			case 'h':
				print_usage(argv);
				return false;

			case 'i':
				delim1 = strrchr(optarg, '/');
				if (delim1 == NULL) {
					incoming_new(NULL, optarg, backend_new_fd, NULL);
				} else {
					*delim1 = '\0';
					delim1++;
					incoming_new(optarg, delim1, backend_new_fd, NULL);
				}
				break;

			case 'l':
				delim1 = strchr(optarg, '=');
				if (delim1 == NULL) {
					print_usage(argv);
					return false;
				}
				*delim1 = '\0';
				delim1++;
				struct serializer *serializer = client_get_serializer(optarg);
				if (!serializer) {
					fprintf(stderr, "Unknown format: %s\n", optarg);
					return false;
				}

				delim2 = strrchr(delim1, '/');
				if (delim2 == NULL) {
					incoming_new(NULL, delim1, client_add_wrapper, serializer);
				} else {
					*delim2 = '\0';
					delim2++;
					incoming_new(delim1, delim2, client_add_wrapper, serializer);
				}
				break;

			default:
				print_usage(argv);
				return false;
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Not a flag: %s\n", argv[optind]);
		print_usage(argv);
		return false;
	}

	return true;
}

int main(int argc, char *argv[]) {
	server_init();
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
