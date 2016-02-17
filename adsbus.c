#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/epoll.h>
#include <string.h>

#include "common.h"
#include "backend.h"
#include "client.h"
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
			, argv[0]);
}

static bool parse_opts(int argc, char *argv[], int epoll_fd) {
	static struct option long_options[] = {
		{"backend", required_argument, 0, 'b'},
		{"dump",    required_argument, 0, 'd'},
		{"help",    no_argument,       0, 'h'},
	};

	int opt;
	char *delim;
	while ((opt = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
		switch (opt) {
			case 'b':
				// It would be really nice if libc had a standard way to split host:port.
				delim = strrchr(optarg, '/');
				if (delim == NULL) {
					print_usage(argv);
					return false;
				}
				*delim = '\0';
				delim++;

				// TODO: Fix orphan malloc.
				struct backend *backend = backend_new(optarg, delim, epoll_fd);
				assert(backend);
				break;

		  case 'd':
				if (!add_dump(optarg)) {
					return false;
				}
				break;

			case 'h':
				print_usage(argv);
				return false;

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

static int loop(int epoll_fd) {
	while (1) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			return -1;
		}

    for (int n = 0; n < nfds; n++) {
			struct peer *peer = events[n].data.ptr;
			peer->event_handler(peer, epoll_fd);
		}
	}
}

int main(int argc, char *argv[]) {
	server_init();
	hex_init();
	airspy_adsb_init();
	beast_init();
	json_init();
	stats_init();

	int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
		perror("epoll_create1");
		return EXIT_FAILURE;
  }

	if (!parse_opts(argc, argv, epoll_fd)) {
		return EXIT_FAILURE;
	}

	loop(epoll_fd);
	close(epoll_fd);
	return EXIT_SUCCESS;
}
