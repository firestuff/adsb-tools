#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "buf.h"
#include "flow.h"
#include "log.h"
#include "peer.h"
#include "resolve.h"
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
	uint32_t attempt;
	struct flow *flow;
	void *passthrough;
	struct list_head outgoing_list;
};

static struct list_head outgoing_head = LIST_HEAD_INIT(outgoing_head);

static void outgoing_connect_result(struct outgoing *, int);
static void outgoing_resolve(struct outgoing *);
static void outgoing_resolve_wrapper(struct peer *);

static void outgoing_retry(struct outgoing *outgoing) {
	uint32_t delay = wakeup_get_retry_delay_ms(++outgoing->attempt);
	log_write('O', outgoing->id, "Will retry in %ds", delay / 1000);
	outgoing->peer.event_handler = outgoing_resolve_wrapper;
	wakeup_add((struct peer *) outgoing, delay);
}

static void outgoing_connect_next(struct outgoing *outgoing) {
	if (outgoing->addr == NULL) {
		freeaddrinfo(outgoing->addrs);
		log_write('O', outgoing->id, "Can't connect to any addresses of %s/%s", outgoing->node, outgoing->service);
		outgoing_retry(outgoing);
		return;
	}

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	log_write('O', outgoing->id, "Connecting to %s/%s...", hbuf, sbuf);

	outgoing->peer.fd = socket(outgoing->addr->ai_family, outgoing->addr->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, outgoing->addr->ai_protocol);
	assert(outgoing->peer.fd >= 0);

	struct buf buf = BUF_INIT, *buf_ptr = &buf;
	flow_get_hello(outgoing->flow, &buf_ptr, outgoing->passthrough);
	ssize_t result = sendto(outgoing->peer.fd, buf_at(buf_ptr, 0), buf_ptr->length, MSG_FASTOPEN, outgoing->addr->ai_addr, outgoing->addr->ai_addrlen);
	outgoing_connect_result(outgoing, result == (ssize_t) buf_ptr->length ? EINPROGRESS : errno);
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
	log_write('O', outgoing->id, "Peer disconnected; reconnecting...");
	outgoing_retry(outgoing);
}

static void outgoing_connect_result(struct outgoing *outgoing, int result) {
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(outgoing->addr->ai_addr, outgoing->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	switch (result) {
		case 0:
			log_write('O', outgoing->id, "Connected to %s/%s", hbuf, sbuf);
			freeaddrinfo(outgoing->addrs);
			outgoing->attempt = 0;
			int fd = outgoing->peer.fd;
			outgoing->peer.fd = -1;
			outgoing->peer.event_handler = outgoing_disconnect_handler;
			flow_socket_ready(fd, outgoing->flow);
			flow_socket_connected(fd, outgoing->flow);
			flow_new(fd, outgoing->flow, outgoing->passthrough, (struct peer *) outgoing);
			break;

		case EINPROGRESS:
			outgoing->peer.event_handler = outgoing_connect_handler;
			peer_epoll_add((struct peer *) outgoing, EPOLLOUT);
			break;

		default:
			log_write('O', outgoing->id, "Can't connect to %s/%s: %s", hbuf, sbuf, strerror(result));
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
	int err = resolve_result(peer, &outgoing->addrs);
	if (err) {
		log_write('O', outgoing->id, "Failed to resolve %s/%s: %s", outgoing->node, outgoing->service, gai_strerror(err));
		outgoing_retry(outgoing);
	} else {
		outgoing->addr = outgoing->addrs;
		outgoing_connect_next(outgoing);
	}
}

static void outgoing_resolve(struct outgoing *outgoing) {
	log_write('O', outgoing->id, "Resolving %s/%s...", outgoing->node, outgoing->service);
	outgoing->peer.event_handler = outgoing_resolve_handler;
	resolve((struct peer *) outgoing, outgoing->node, outgoing->service, 0);
}

static void outgoing_resolve_wrapper(struct peer *peer) {
	outgoing_resolve((struct outgoing *) peer);
}

static void outgoing_del(struct outgoing *outgoing) {
	flow_ref_dec(outgoing->flow);
	if (outgoing->peer.fd >= 0) {
		assert(!close(outgoing->peer.fd));
	}
	list_del(&outgoing->outgoing_list);
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

void outgoing_new(const char *node, const char *service, struct flow *flow, void *passthrough) {
	flow_ref_inc(flow);

	struct outgoing *outgoing = malloc(sizeof(*outgoing));
	uuid_gen(outgoing->id);
	outgoing->node = strdup(node);
	outgoing->service = strdup(service);
	outgoing->attempt = 0;
	outgoing->flow = flow;
	outgoing->passthrough = passthrough;

	list_add(&outgoing->outgoing_list, &outgoing_head);

	outgoing_resolve(outgoing);
}
