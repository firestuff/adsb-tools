#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/epoll.h>

#include "common.h"
#include "airspy_adsb.h"


struct opts {
	char *backend_node;
	char *backend_service;
};

struct client {
	int placeholder;
};


static int parse_opts(int argc, char *argv[], struct opts *opts) {
	int opt;
	while ((opt = getopt(argc, argv, "h:p:")) != -1) {
		switch (opt) {
		  case 'h':
			  opts->backend_node = optarg;
				break;

			case 'p':
				opts->backend_service = optarg;
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
		.backend_node = "localhost",
		.backend_service = "30006",
	};
	if (parse_opts(argc, argv, &opts)) {
		fprintf(stderr, "Usage: %s [-h backend_host] [-p backend_port]\n", argv[0]);
		return EXIT_FAILURE;
	}

	int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
		perror("epoll_create1");
		return EXIT_FAILURE;
  }

	struct backend backend = BACKEND_INIT;
	if (!backend_connect(opts.backend_node, opts.backend_service, &backend, epoll_fd)) {
		fprintf(stderr, "Unable to connect to %s/%s\n", opts.backend_node, opts.backend_service);
		return EXIT_FAILURE;
	}

	loop(epoll_fd);
	close(epoll_fd);
	return EXIT_SUCCESS;
}
