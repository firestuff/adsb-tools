#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "outgoing.h"

struct outgoing;
struct outgoing {
	struct peer peer;
	char id[UUID_LEN];
	char *node;
	char *service;
	struct addrinfo *addrs;
	struct addrinfo *addr;
	outgoing_connection_handler handler;
	void *passthrough;
	struct outgoing *next;
};

static struct outgoing *outgoing_head = NULL;

static void outgoing_connect_result(struct outgoing *, int);
static void outgoing_resolve(struct outgoing *);

static void outgoing_connect_next(struct outgoing *outgoing) {
	if (outgoing->addr == NULL) {
		freeaddrinfo(outgoing->addrs);
		fprintf(stderr, "O %s: Can't connect to any addresses of %s/%s\n", outgoing->id, outgoing->node, outgoing->service);
		// TODO: timed retry
		return;
	}

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	fprintf(stderr, "O %s: Connecting to %s/%s...\n", outgoing->id, hbuf, sbuf);

	outgoing->peer.fd = socket(outgoing->addr->ai_family, outgoing->addr->ai_socktype | SOCK_NONBLOCK, outgoing->addr->ai_protocol);
	assert(outgoing->peer.fd >= 0);

	int result = connect(outgoing->peer.fd, outgoing->addr->ai_addr, outgoing->addr->ai_addrlen);
	outgoing_connect_result(outgoing, result == 0 ? result : errno);
}

static void outgoing_connect_handler(struct peer *peer) {
	struct outgoing *outgoing = (struct outgoing *) peer;

	peer_epoll_del(&outgoing->peer);

	int error;
  socklen_t len = sizeof(error);
	assert(getsockopt(outgoing->peer.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0);
	outgoing_connect_result(outgoing, error);
}

static void outgoing_disconnect_handler(struct peer *peer) {
	struct outgoing *outgoing = (struct outgoing *) peer;
	assert(!close(outgoing->peer.fd));
	fprintf(stderr, "O %s: Peer disconnected; reconnecting...\n", outgoing->id);

	outgoing_resolve(outgoing);
}

static void outgoing_connect_result(struct outgoing *outgoing, int result) {
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	switch (result) {
		case 0:
			fprintf(stderr, "O %s: Connected to %s/%s\n", outgoing->id, hbuf, sbuf);
			freeaddrinfo(outgoing->addrs);

			// We listen for just hangup on this fd. We pass a duplicate to the handler, so our epoll setings are independent.
			outgoing->peer.event_handler = outgoing_disconnect_handler;
			peer_epoll_add((struct peer *) outgoing, EPOLLRDHUP);

			outgoing->handler(dup(outgoing->peer.fd), outgoing->passthrough);
			break;

		case EINPROGRESS:
			outgoing->peer.event_handler = outgoing_connect_handler;
			peer_epoll_add((struct peer *) outgoing, EPOLLOUT);
			break;

		default:
			fprintf(stderr, "O %s: Can't connect to %s/%s: %s\n", outgoing->id, hbuf, sbuf, strerror(result));
			assert(!close(outgoing->peer.fd));
			outgoing->addr = outgoing->addr->ai_next;
			// Tail recursion :/
			outgoing_connect_next(outgoing);
			break;
	}
}

static void outgoing_resolve(struct outgoing *outgoing) {
	fprintf(stderr, "O %s: Resolving %s/%s...\n", outgoing->id, outgoing->node, outgoing->service);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	int gai_err = getaddrinfo(outgoing->node, outgoing->service, &hints, &outgoing->addrs);
	if (gai_err) {
		fprintf(stderr, "O %s: Failed to resolve %s/%s: %s\n", outgoing->id, outgoing->node, outgoing->service, gai_strerror(gai_err));
		// TODO: timed retry
		return;
	}
	outgoing->addr = outgoing->addrs;
	outgoing_connect_next(outgoing);
}

static void outgoing_del(struct outgoing *outgoing) {
	assert(!close(outgoing->peer.fd));
	free(outgoing->node);
	free(outgoing->service);
	free(outgoing);
}

void outgoing_cleanup() {
	struct outgoing *iter = outgoing_head;
	while (iter) {
		struct outgoing *next = iter->next;
		outgoing_del(iter);
		iter = next;
	}
}

void outgoing_new(char *node, char *service, outgoing_connection_handler handler, void *passthrough) {
	struct outgoing *outgoing = malloc(sizeof(*outgoing));
	uuid_gen(outgoing->id);
	outgoing->node = strdup(node);
	outgoing->service = strdup(service);
	outgoing->handler = handler;
	outgoing->passthrough = passthrough;

	outgoing->next = outgoing_head;
	outgoing_head = outgoing;

	outgoing_resolve(outgoing);
}
