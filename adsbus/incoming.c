#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "buf.h"
#include "list.h"
#include "peer.h"
#include "resolve.h"
#include "socket.h"
#include "wakeup.h"
#include "uuid.h"

#include "incoming.h"

struct incoming {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *node;
	char *service;
	struct addrinfo *addrs;
	const char *error;
	uint32_t attempt;
	incoming_connection_handler handler;
	incoming_get_hello hello;
	void *passthrough;
	uint32_t *count;
	struct list_head incoming_list;
};

static struct list_head incoming_head = LIST_HEAD_INIT(incoming_head);

static void incoming_resolve_wrapper(struct peer *);

static void incoming_retry(struct incoming *incoming) {
	uint32_t delay = wakeup_get_retry_delay_ms(incoming->attempt++);
	fprintf(stderr, "I %s: Will retry in %ds\n", incoming->id, delay / 1000);
	incoming->peer.event_handler = incoming_resolve_wrapper;
	wakeup_add((struct peer *) incoming, delay);
}

static bool incoming_hello(int fd, struct incoming *incoming) {
	if (!incoming->hello) {
		return true;
	}
	struct buf buf = BUF_INIT, *buf_ptr = &buf;
	incoming->hello(&buf_ptr, incoming->passthrough);
	if (!buf_ptr->length) {
		return true;
	}
	return (write(fd, buf_at(buf_ptr, 0), buf_ptr->length) == (ssize_t) buf_ptr->length);
}

static void incoming_handler(struct peer *peer) {
	struct incoming *incoming = (struct incoming *) peer;

	struct sockaddr peer_addr, local_addr;
	socklen_t peer_addrlen = sizeof(peer_addr), local_addrlen = sizeof(local_addr);

	int fd = accept4(incoming->peer.fd, &peer_addr, &peer_addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd == -1) {
		fprintf(stderr, "I %s: Failed to accept new connection on %s/%s: %s\n", incoming->id, incoming->node, incoming->service, strerror(errno));
		return;
	}

	char peer_hbuf[NI_MAXHOST], local_hbuf[NI_MAXHOST], peer_sbuf[NI_MAXSERV], local_sbuf[NI_MAXSERV];
	assert(getsockname(fd, &local_addr, &local_addrlen) == 0);
	assert(getnameinfo(&peer_addr, peer_addrlen, peer_hbuf, sizeof(peer_hbuf), peer_sbuf, sizeof(peer_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	assert(getnameinfo(&local_addr, local_addrlen, local_hbuf, sizeof(local_hbuf), local_sbuf, sizeof(local_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);

	fprintf(stderr, "I %s: New incoming connection on %s/%s (%s/%s) from %s/%s\n",
			incoming->id,
			incoming->node, incoming->service,
			local_hbuf, local_sbuf,
			peer_hbuf, peer_sbuf);

	socket_connected_init(fd);

	if (!incoming_hello(fd, incoming)) {
		fprintf(stderr, "I %s: Error writing greeting\n", incoming->id);
		assert(!close(fd));
		return;
	}

	incoming->handler(fd, incoming->passthrough, NULL);
}

static void incoming_del(struct incoming *incoming) {
	(*incoming->count)--;
	if (incoming->peer.fd >= 0) {
		assert(!close(incoming->peer.fd));
	}
	free(incoming->node);
	free(incoming->service);
	free(incoming);
}

static void incoming_listen(struct incoming *incoming) {
	struct addrinfo *addr;
	for (addr = incoming->addrs; addr; addr = addr->ai_next) {
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
		assert(getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
		fprintf(stderr, "I %s: Listening on %s/%s...\n", incoming->id, hbuf, sbuf);

		incoming->peer.fd = socket(addr->ai_family, addr->ai_socktype | SOCK_CLOEXEC, addr->ai_protocol);
		assert(incoming->peer.fd >= 0);

		socket_pre_bind_init(incoming->peer.fd);

		if (bind(incoming->peer.fd, addr->ai_addr, addr->ai_addrlen) != 0) {
			fprintf(stderr, "I %s: Failed to bind to %s/%s: %s\n", incoming->id, hbuf, sbuf, strerror(errno));
			assert(!close(incoming->peer.fd));
			continue;
		}

		socket_bound_init(incoming->peer.fd);

		assert(listen(incoming->peer.fd, 255) == 0);
		break;
	}

	freeaddrinfo(incoming->addrs);

	if (addr == NULL) {
		fprintf(stderr, "I %s: Failed to bind any addresses for %s/%s...\n", incoming->id, incoming->node, incoming->service);
		incoming_retry(incoming);
		return;
	}

	incoming->attempt = 0;
	incoming->peer.event_handler = incoming_handler;
	peer_epoll_add((struct peer *) incoming, EPOLLIN);
}

static void incoming_resolve_handler(struct peer *peer) {
	struct incoming *incoming = (struct incoming *) peer;
	if (incoming->addrs) {
		incoming_listen(incoming);
	} else {
		fprintf(stderr, "I %s: Failed to resolve %s/%s: %s\n", incoming->id, incoming->node, incoming->service, incoming->error);
		incoming_retry(incoming);
	}
}

static void incoming_resolve(struct incoming *incoming) {
	fprintf(stderr, "I %s: Resolving %s/%s...\n", incoming->id, incoming->node, incoming->service);
	incoming->peer.fd = -1;
	incoming->peer.event_handler = incoming_resolve_handler;
	resolve((struct peer *) incoming, incoming->node, incoming->service, AI_PASSIVE, &incoming->addrs, &incoming->error);
}

static void incoming_resolve_wrapper(struct peer *peer) {
	incoming_resolve((struct incoming *) peer);
}

void incoming_cleanup() {
	struct incoming *iter, *next;
	list_for_each_entry_safe(iter, next, &incoming_head, incoming_list) {
		incoming_del(iter);
	}
}

void incoming_new(char *node, char *service, incoming_connection_handler handler, incoming_get_hello hello, void *passthrough, uint32_t *count) {
	(*count)++;

	struct incoming *incoming = malloc(sizeof(*incoming));
	incoming->peer.event_handler = incoming_handler;
	uuid_gen(incoming->id);
	incoming->node = strdup(node);
	incoming->service = strdup(service);
	incoming->attempt = 0;
	incoming->handler = handler;
	incoming->hello = hello;
	incoming->passthrough = passthrough;
	incoming->count = count;

	list_add(&incoming->incoming_list, &incoming_head);

	incoming_resolve(incoming);
}
