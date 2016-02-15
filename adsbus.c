#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <sys/epoll.h>

#include "common.h"
#include "airspy_adsb.h"


struct opts {
	char *backend_node;
	char *backend_service;
};

typedef bool (*parser)(struct buf *, struct packet *, void *);
static parser parsers[] = {
	airspy_adsb_parse,
};
#define NUM_PARSERS (sizeof(parsers) / sizeof(*parsers))

struct backend {
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser parser;
};

struct client {
	int placeholder;
};

struct peer {
	enum {
		BACKEND,
		CLIENT,
	} type;
	int fd;
	union {
		struct backend backend;
		struct client client;
	};
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


static int connect_backend(struct opts *opts) {
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *addrs;

	int gai_err = getaddrinfo(opts->backend_node, opts->backend_service, &hints, &addrs);
	if (gai_err) {
		fprintf(stderr, "getaddrinfo(%s/%s): %s\n", opts->backend_node, opts->backend_service, gai_strerror(gai_err));
		return -1;
	}

	int bfd;
	struct addrinfo *addr;
  for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		bfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (bfd == -1) {
			perror("socket");
			continue;
		}

		if (connect(bfd, addr->ai_addr, addr->ai_addrlen) != -1) {
			break;
		}

		close(bfd);
	}

	if (addr == NULL) {
		freeaddrinfo(addrs);
		fprintf(stderr, "Can't connect to %s/%s\n", opts->backend_node, opts->backend_service);
		return -1;
	}

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  if (getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
		fprintf(stderr, "Connected to %s/%s\n", hbuf, sbuf);
	}

	freeaddrinfo(addrs);

	return bfd;
}


static int loop(int bfd) {
	struct peer backend = {
		.type = BACKEND,
		.fd = bfd,
		.backend = {
			.buf = {
				.start = 0,
				.length = 0,
			},
			.parser_state = { 0 },
			.parser = NULL,
		},
	};

	int efd = epoll_create1(0);
  if (efd == -1) {
		perror("epoll_create1");
		return -1;
  }

	{
		struct epoll_event ev = {
			.events = EPOLLIN,
			.data = {
				.ptr = &backend,
			},
		};
		if (epoll_ctl(efd, EPOLL_CTL_ADD, bfd, &ev) == -1) {
			perror("epoll_ctl");
			return -1;
		}
	}

	while (1) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			return -1;
		}

    for (int n = 0; n < nfds; n++) {
			struct peer *peer = events[n].data.ptr;
			switch (peer->type) {
				case BACKEND:
					if (buf_fill(&peer->backend.buf, peer->fd) < 0) {
						fprintf(stderr, "Connection closed by backend\n");
						return -1;
					}

					struct packet packet;
					if (!peer->backend.parser) {
						// Attempt to autodetect format
						for (int i = 0; i < NUM_PARSERS; i++) {
							if (parsers[i](&peer->backend.buf, &packet, peer->backend.parser_state)) {
								peer->backend.parser = parsers[i];
								break;
							}
						}
					}

					if (peer->backend.parser) {
						while (peer->backend.parser(&peer->backend.buf, &packet, peer->backend.parser_state)) {
						}
					}

					if (peer->backend.buf.length == BUF_LEN_MAX) {
						fprintf(stderr, "Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.\n");
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

	int bfd = connect_backend(&opts);
	if (bfd < 0) {
		fprintf(stderr, "Unable to connect to %s/%s\n", opts.backend_node, opts.backend_service);
		return EXIT_FAILURE;
	}

	loop(bfd);
	close(bfd);
	return EXIT_SUCCESS;
}
