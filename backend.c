#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <string.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "client.h"
#include "backend.h"


bool backend_autodetect_parse(struct backend *, struct packet *);


static parser parsers[] = {
	airspy_adsb_parse,
	beast_parse,
};
#define NUM_PARSERS (sizeof(parsers) / sizeof(*parsers))


void backend_init(struct backend *backend) {
	backend->type = PEER_BACKEND;
	backend->fd = -1;
	buf_init(&backend->buf);
	memset(backend->parser_state, 0, PARSER_STATE_LEN);
	backend->parser = backend_autodetect_parse;
}

bool backend_connect(char *node, char *service, struct backend *backend, int epoll_fd) {
	assert(backend->type == PEER_BACKEND);

	uuid_gen(backend->id);

	struct addrinfo *addrs;

	{
		struct addrinfo hints = {
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
		};

		int gai_err = getaddrinfo(node, service, &hints, &addrs);
		if (gai_err) {
			fprintf(stderr, "B %s: getaddrinfo(%s %s): %s\n", backend->id, node, service, gai_strerror(gai_err));
			return false;
		}
	}

	{
		struct addrinfo *addr;
  	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
			backend->fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
			if (backend->fd == -1) {
				perror("socket");
				continue;
			}

			if (connect(backend->fd, addr->ai_addr, addr->ai_addrlen) != -1) {
				break;
			}

			close(backend->fd);
		}

		if (addr == NULL) {
			freeaddrinfo(addrs);
			fprintf(stderr, "B %s: Can't connect to %s %s\n", backend->id, node, service);
			return false;
		}

  	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  	if (getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
			fprintf(stderr, "B %s: Connected to %s %s\n", backend->id, hbuf, sbuf);
		}
	}

	freeaddrinfo(addrs);

	{
		struct epoll_event ev = {
			.events = EPOLLIN,
			.data = {
				.ptr = backend,
			},
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, backend->fd, &ev) == -1) {
			perror("epoll_ctl");
			return false;
		}
	}

	return true;
}

bool backend_read(struct backend *backend) {
	if (buf_fill(&backend->buf, backend->fd) < 0) {
		fprintf(stderr, "B %s: Connection closed by backend\n", backend->id);
		return false;
	}

	struct packet packet = {
		.backend = backend,
	};
	while (backend->parser(backend, &packet)) {
		client_write(&packet);
	}

	if (backend->buf.length == BUF_LEN_MAX) {
		fprintf(stderr, "B %s: Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.\n", backend->id);
		return false;
	}
	return true;
}

bool backend_autodetect_parse(struct backend *backend, struct packet *packet) {
	assert(backend->type == PEER_BACKEND);

	for (int i = 0; i < NUM_PARSERS; i++) {
		if (parsers[i](backend, packet)) {
			backend->parser = parsers[i];
			return true;
		}
	}
	return false;
}
