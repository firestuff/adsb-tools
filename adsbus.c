#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/epoll.h>

#include "common.h"
#include "airspy_adsb.h"


struct opts {
	bool dump;
};

struct client {
	int placeholder;
};


static int parse_opts(int argc, char *argv[], struct opts *opts) {
	int opt;
	while ((opt = getopt(argc, argv, "d")) != -1) {
		switch (opt) {
		  case 'd':
			  opts->dump = true;
				break;

			default:
				return -1;
		}
	}
	return 0;
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
			switch (peer->type) {
				case PEER_BACKEND:
					if (!backend_read((struct backend *) peer)) {
						return -1;
					}
					break;

				default:
					fprintf(stderr, "Unpossible: unknown peer type.\n");
					return -1;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	hex_init();
	airspy_adsb_init();

	struct opts opts = {
		.dump = false,
	};
	if (parse_opts(argc, argv, &opts) ||
	    argc - optind < 2 ||
			(argc - optind) % 2 != 0) {
		fprintf(stderr, "Usage: %s -d localhost 30006 [ remotehost 30002 ... ]\n", argv[0]);
		return EXIT_FAILURE;
	}

	int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
		perror("epoll_create1");
		return EXIT_FAILURE;
  }

	int nbackends = (argc - optind) / 2;
	struct backend backends[nbackends];
	for (int i = 0, j = optind; i < nbackends && j < argc; i++, j += 2) {
		backend_init(&backends[i]);
		if (!backend_connect(argv[j], argv[j + 1], &backends[i], epoll_fd)) {
			return EXIT_FAILURE;
		}
	}

	loop(epoll_fd);
	close(epoll_fd);
	return EXIT_SUCCESS;
}
