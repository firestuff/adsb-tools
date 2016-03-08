#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "flow.h"
#include "log.h"
#include "opts.h"
#include "peer.h"
#include "receive.h"
#include "resolve.h"
#include "send.h"
#include "send_receive.h"
#include "socket.h"
#include "wakeup.h"
#include "uuid.h"

#include "incoming.h"

struct incoming {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *node;
	char *service;
	uint32_t attempt;
	struct flow *flow;
	void *passthrough;
	struct list_head incoming_list;
};

static struct list_head incoming_head = LIST_HEAD_INIT(incoming_head);
static opts_group incoming_opts;

static char log_module = 'I';

static void incoming_resolve_wrapper(struct peer *);

static void incoming_retry(struct incoming *incoming) {
	uint32_t delay = wakeup_get_retry_delay_ms(incoming->attempt++);
	LOG(incoming->id, "Will retry in %ds", delay / 1000);
	incoming->peer.event_handler = incoming_resolve_wrapper;
	wakeup_add((struct peer *) incoming, delay);
}

static void incoming_handler(struct peer *peer) {
	struct incoming *incoming = (struct incoming *) peer;

	struct sockaddr peer_addr, local_addr;
	socklen_t peer_addrlen = sizeof(peer_addr), local_addrlen = sizeof(local_addr);

	int fd = accept4(incoming->peer.fd, &peer_addr, &peer_addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd == -1) {
		LOG(incoming->id, "Failed to accept new connection on %s/%s: %s", incoming->node, incoming->service, strerror(errno));
		return;
	}

	char peer_hbuf[NI_MAXHOST], local_hbuf[NI_MAXHOST], peer_sbuf[NI_MAXSERV], local_sbuf[NI_MAXSERV];
	assert(getsockname(fd, &local_addr, &local_addrlen) == 0);
	assert(getnameinfo(&peer_addr, peer_addrlen, peer_hbuf, sizeof(peer_hbuf), peer_sbuf, sizeof(peer_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	assert(getnameinfo(&local_addr, local_addrlen, local_hbuf, sizeof(local_hbuf), local_sbuf, sizeof(local_sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);

	LOG(incoming->id, "New incoming connection on %s/%s (%s/%s) from %s/%s",
			incoming->node, incoming->service,
			local_hbuf, local_sbuf,
			peer_hbuf, peer_sbuf);

	flow_socket_connected(fd, incoming->flow);

	if (!flow_new_send_hello(fd, incoming->flow, incoming->passthrough, NULL)) {
		LOG(incoming->id, "Error writing greeting");
		return;
	}
}

static void incoming_del(struct incoming *incoming) {
	flow_ref_dec(incoming->flow);
	if (incoming->peer.fd >= 0) {
		assert(!close(incoming->peer.fd));
	}
	list_del(&incoming->incoming_list);
	free(incoming->node);
	free(incoming->service);
	free(incoming);
}

static void incoming_listen(struct peer *peer) {
	struct incoming *incoming = (struct incoming *) peer;

	struct addrinfo *addrs;
	int err = resolve_result(peer, &addrs);
	if (err) {
		LOG(incoming->id, "Failed to resolve %s/%s: %s", incoming->node, incoming->service, gai_strerror(err));
		incoming_retry(incoming);
		return;
	}

	struct addrinfo *addr;
	for (addr = addrs; addr; addr = addr->ai_next) {
		char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
		assert(getnameinfo(addr->ai_addr, addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
		LOG(incoming->id, "Listening on %s/%s...", hbuf, sbuf);

		incoming->peer.fd = socket(addr->ai_family, addr->ai_socktype | SOCK_CLOEXEC, addr->ai_protocol);
		assert(incoming->peer.fd >= 0);

		socket_pre_bind(incoming->peer.fd);

		if (bind(incoming->peer.fd, addr->ai_addr, addr->ai_addrlen) != 0) {
			LOG(incoming->id, "Failed to bind to %s/%s: %s", hbuf, sbuf, strerror(errno));
			assert(!close(incoming->peer.fd));
			continue;
		}

		socket_pre_listen(incoming->peer.fd);
		// Options are inherited through accept()
		flow_socket_ready(incoming->peer.fd, incoming->flow);

		assert(listen(incoming->peer.fd, 255) == 0);
		break;
	}

	freeaddrinfo(addrs);

	if (addr == NULL) {
		LOG(incoming->id, "Failed to bind any addresses for %s/%s...", incoming->node, incoming->service);
		incoming_retry(incoming);
		return;
	}

	incoming->attempt = 0;
	incoming->peer.event_handler = incoming_handler;
	peer_epoll_add((struct peer *) incoming, EPOLLIN);
}

static void incoming_resolve(struct incoming *incoming) {
	LOG(incoming->id, "Resolving %s/%s...", incoming->node, incoming->service);
	incoming->peer.event_handler = incoming_listen;
	resolve((struct peer *) incoming, incoming->node, incoming->service, AI_PASSIVE);
}

static void incoming_resolve_wrapper(struct peer *peer) {
	incoming_resolve((struct incoming *) peer);
}

static bool incoming_add(const char *host_port, struct flow *flow, void *passthrough) {
	char *host = opts_split(&host_port, '/');
	if (host) {
		incoming_new(host, host_port, flow, passthrough);
		free(host);
	} else {
		incoming_new(NULL, host_port, flow, passthrough);
	}
	return true;
}

static bool incoming_listen_receive(const char *arg) {
	return incoming_add(arg, receive_flow, NULL);
}

static bool incoming_listen_send(const char *arg) {
	return send_add(incoming_add, send_flow, arg);
}

static bool incoming_listen_send_receive(const char *arg) {
	return send_add(incoming_add, send_receive_flow, arg);
}

void incoming_opts_add() {
	opts_add("listen-receive", "[HOST/]PORT", incoming_listen_receive, incoming_opts);
	opts_add("listen-send", "FORMAT=[HOST/]PORT", incoming_listen_send, incoming_opts);
	opts_add("listen-send-receive", "FORMAT=[HOST/]PORT", incoming_listen_send_receive, incoming_opts);
}

void incoming_init() {
	opts_call(incoming_opts);
}

void incoming_cleanup() {
	struct incoming *iter, *next;
	list_for_each_entry_safe(iter, next, &incoming_head, incoming_list) {
		incoming_del(iter);
	}
}

void incoming_new(const char *node, const char *service, struct flow *flow, void *passthrough) {
	flow_ref_inc(flow);

	struct incoming *incoming = malloc(sizeof(*incoming));
	incoming->peer.event_handler = incoming_handler;
	uuid_gen(incoming->id);
	incoming->node = node ? strdup(node) : NULL;
	incoming->service = strdup(service);
	incoming->attempt = 0;
	incoming->flow = flow;
	incoming->passthrough = passthrough;

	list_add(&incoming->incoming_list, &incoming_head);

	incoming_resolve(incoming);
}
