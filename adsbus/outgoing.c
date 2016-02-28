#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "list.h"
#include "peer.h"
#include "resolve.h"
#include "socket.h"
#include "wakeup.h"
#include "uuid.h"

#include "outgoing.h"

struct outgoing {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *node;
	char *service;
	struct addrinfo *addrs;
	struct addrinfo *addr;
	const char *error;
	uint32_t attempt;
	outgoing_connection_handler handler;
	outgoing_get_hello hello;
	void *passthrough;
	uint32_t *count;
	struct list_head outgoing_list;
};

static struct list_head outgoing_head = LIST_HEAD_INIT(outgoing_head);

static void outgoing_connect_result(struct outgoing *, int);
static void outgoing_resolve(struct outgoing *);
static void outgoing_resolve_wrapper(struct peer *);

static void outgoing_retry(struct outgoing *outgoing) {
	uint32_t delay = wakeup_get_retry_delay_ms(++outgoing->attempt);
	fprintf(stderr, "O %s: Will retry in %ds\n", outgoing->id, delay / 1000);
	outgoing->peer.event_handler = outgoing_resolve_wrapper;
	wakeup_add((struct peer *) outgoing, delay);
}

static void outgoing_connect_next(struct outgoing *outgoing) {
	if (outgoing->addr == NULL) {
		freeaddrinfo(outgoing->addrs);
		fprintf(stderr, "O %s: Can't connect to any addresses of %s/%s\n", outgoing->id, outgoing->node, outgoing->service);
		outgoing_retry(outgoing);
		return;
	}

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	fprintf(stderr, "O %s: Connecting to %s/%s...\n", outgoing->id, hbuf, sbuf);

	outgoing->peer.fd = socket(outgoing->addr->ai_family, outgoing->addr->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, outgoing->addr->ai_protocol);
	assert(outgoing->peer.fd >= 0);

	struct buf buf = BUF_INIT, *buf_ptr = &buf;
	if (outgoing->hello) {
		outgoing->hello(&buf_ptr, outgoing->passthrough);
	}
	int result = (int) sendto(outgoing->peer.fd, buf_at(buf_ptr, 0), buf_ptr->length, MSG_FASTOPEN, outgoing->addr->ai_addr, outgoing->addr->ai_addrlen);
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
	if (outgoing->peer.fd != -1) {
		assert(!close(outgoing->peer.fd));
	}
	fprintf(stderr, "O %s: Peer disconnected; reconnecting...\n", outgoing->id);
	outgoing_retry(outgoing);
}

static void outgoing_connect_result(struct outgoing *outgoing, int result) {
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	switch (result) {
		case 0:
			fprintf(stderr, "O %s: Connected to %s/%s\n", outgoing->id, hbuf, sbuf);
			freeaddrinfo(outgoing->addrs);
			socket_connected_init(outgoing->peer.fd);
			outgoing->attempt = 0;
			int fd = outgoing->peer.fd;
			outgoing->peer.fd = -1;
			outgoing->peer.event_handler = outgoing_disconnect_handler;
			outgoing->handler(fd, outgoing->passthrough, (struct peer *) outgoing);
			break;

		case EINPROGRESS:
			outgoing->peer.event_handler = outgoing_connect_handler;
			peer_epoll_add((struct peer *) outgoing, EPOLLOUT);
			break;

		default:
			fprintf(stderr, "O %s: Can't connect to %s/%s: %s\n", outgoing->id, hbuf, sbuf, strerror(result));
			assert(!close(outgoing->peer.fd));
			outgoing->peer.fd = -1;
			outgoing->addr = outgoing->addr->ai_next;
			// Tail recursion :/
			outgoing_connect_next(outgoing);
			break;
	}
}

static void outgoing_resolve_handler(struct peer *peer) {
	struct outgoing *outgoing = (struct outgoing *) peer;
	if (outgoing->addrs) {
		outgoing->addr = outgoing->addrs;
		outgoing_connect_next(outgoing);
	} else {
		fprintf(stderr, "O %s: Failed to resolve %s/%s: %s\n", outgoing->id, outgoing->node, outgoing->service, outgoing->error);
		outgoing_retry(outgoing);
	}
}

static void outgoing_resolve(struct outgoing *outgoing) {
	fprintf(stderr, "O %s: Resolving %s/%s...\n", outgoing->id, outgoing->node, outgoing->service);
	outgoing->peer.fd = -1;
	outgoing->peer.event_handler = outgoing_resolve_handler;
	resolve((struct peer *) outgoing, outgoing->node, outgoing->service, 0, &outgoing->addrs, &outgoing->error);
}

static void outgoing_resolve_wrapper(struct peer *peer) {
	outgoing_resolve((struct outgoing *) peer);
}

static void outgoing_del(struct outgoing *outgoing) {
	(*outgoing->count)--;
	if (outgoing->peer.fd >= 0) {
		assert(!close(outgoing->peer.fd));
	}
	free(outgoing->node);
	free(outgoing->service);
	free(outgoing);
}

void outgoing_cleanup() {
	struct outgoing *iter, *next;
	list_for_each_entry_safe(iter, next, &outgoing_head, outgoing_list) {
		outgoing_del(iter);
	}
}

void outgoing_new(char *node, char *service, outgoing_connection_handler handler, outgoing_get_hello hello, void *passthrough, uint32_t *count) {
	(*count)++;

	struct outgoing *outgoing = malloc(sizeof(*outgoing));
	uuid_gen(outgoing->id);
	outgoing->node = strdup(node);
	outgoing->service = strdup(service);
	outgoing->attempt = 0;
	outgoing->handler = handler;
	outgoing->hello = hello;
	outgoing->passthrough = passthrough;
	outgoing->count = count;

	list_add(&outgoing->outgoing_list, &outgoing_head);

	outgoing_resolve(outgoing);
}
