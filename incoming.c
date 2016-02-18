#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "incoming.h"

struct incoming {
	struct peer peer;
	char id[UUID_LEN];
	const char *node;
	const char *service;
	incoming_connection_handler handler;
	void *passthrough;
};

static void incoming_handler(struct peer *peer) {
	struct incoming *incoming = (struct incoming *) peer;

	struct sockaddr peer_addr, local_addr;
	socklen_t peer_addrlen = sizeof(peer_addr), local_addrlen = sizeof(local_addr);

	int fd = accept(incoming->peer.fd, &peer_addr, &peer_addrlen);
	if (fd == -1) {
		fprintf(stderr, "I %s: Failed to accept new connection on %s/%s: %s\n", incoming->id, incoming->node, incoming->service, strerror(errno));
		return;
	}

	char peer_hbuf[NI_MAXHOST], local_hbuf[NI_MAXHOST], peer_sbuf[NI_MAXSERV], local_sbuf[NI_MAXSERV];
	assert(getsockname(fd, &local_addr, &local_addrlen) == 0);
	assert(getnameinfo(&peer_addr, peer_addrlen, peer_hbuf, sizeof(peer_hbuf), peer_sbuf, sizeof(peer_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	assert(getnameinfo(&local_addr, local_addrlen, local_hbuf, sizeof(local_hbuf), local_sbuf, sizeof(local_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);

	fprintf(stderr, "I %s: New connection on %s/%s (%s/%s) from %s/%s\n",
			incoming->id,
			incoming->node, incoming->service,
			local_hbuf, local_sbuf,
			peer_hbuf, peer_sbuf);

	incoming->handler(fd, incoming->passthrough);
}

void incoming_new(const char *node, const char *service, incoming_connection_handler handler, void *passthrough) {
	struct incoming *incoming = malloc(sizeof(*incoming));
	incoming->peer.event_handler = incoming_handler;
	uuid_gen(incoming->id);
	incoming->node = node;
	incoming->service = service;
	incoming->handler = handler;
	incoming->passthrough = passthrough;

	fprintf(stderr, "I %s: Resolving %s/%s...\n", incoming->id, incoming->node, incoming->service);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ADDRCONFIG,
	};

	struct addrinfo *addrs;
	int gai_err = getaddrinfo(incoming->node, incoming->service, &hints, &addrs);
	if (gai_err) {
		fprintf(stderr, "I %s: Failed to resolve %s/%s: %s\n", incoming->id, incoming->node, incoming->service, gai_strerror(gai_err));
		free(incoming);
		return;
	}

	struct addrinfo *addr;
	for (addr = addrs; addr; addr = addr->ai_next) {
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
		assert(getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
		fprintf(stderr, "I %s: Listening on %s/%s...\n", incoming->id, hbuf, sbuf);

		incoming->peer.fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		assert(incoming->peer.fd >= 0);

		int optval = 1;
		setsockopt(incoming->peer.fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

		if (bind(incoming->peer.fd, addr->ai_addr, addr->ai_addrlen) != 0) {
			fprintf(stderr, "I %s: Failed to bind to %s/%s: %s\n", incoming->id, hbuf, sbuf, strerror(errno));
			close(incoming->peer.fd);
			continue;
		}

		assert(listen(incoming->peer.fd, 255) == 0);
		break;
	}

	freeaddrinfo(addrs);

	if (addr == NULL) {
		fprintf(stderr, "I %s: Failed to bind any addresses for %s/%s...\n", incoming->id, incoming->node, incoming->service);
		free(incoming);
		return;
	}

	peer_epoll_add((struct peer *) incoming, EPOLLIN);
}
