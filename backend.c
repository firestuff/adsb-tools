#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "airspy_adsb.h"
#include "beast.h"
#include "raw.h"

#include "client.h"
#include "backend.h"

typedef bool (*parser_wrapper)(struct backend *, struct packet *);
typedef bool (*parser)(struct buf *, struct packet *, void *state);
struct backend {
	struct peer peer;
	char id[UUID_LEN];
	const char *node;
	const char *service;
	struct addrinfo *addrs;
	struct addrinfo *addr;
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser_wrapper parser_wrapper;
	parser parser;
};

static void backend_connect_result(struct backend *, int);

struct parser {
	char *name;
	parser parse;
} parsers[] = {
	{
		.name = "airspy_adsb",
		.parse = airspy_adsb_parse,
	},
	{
		.name = "beast",
		.parse = beast_parse,
	},
	{
		.name = "raw",
		.parse = raw_parse,
	},
};
#define NUM_PARSERS (sizeof(parsers) / sizeof(*parsers))

static bool backend_parse_wrapper(struct backend *backend, struct packet *packet) {
	return backend->parser(&backend->buf, packet, backend->parser_state);
}

static bool backend_autodetect_parse(struct backend *backend, struct packet *packet) {
	struct buf *buf = &backend->buf;
	void *state = backend->parser_state;

	for (int i = 0; i < NUM_PARSERS; i++) {
		if (parsers[i].parse(buf, packet, state)) {
			fprintf(stderr, "B %s: Detected input format %s\n", backend->id, parsers[i].name);
			backend->parser_wrapper = backend_parse_wrapper;
			backend->parser = parsers[i].parse;
			return true;
		}
	}
	return false;
}

static struct backend *backend_create() {
	struct backend *backend = malloc(sizeof(*backend));
	assert(backend);
	uuid_gen(backend->id);
	backend->peer.fd = -1;
	backend->node = NULL;
	backend->service = NULL;
	buf_init(&backend->buf);
	memset(backend->parser_state, 0, PARSER_STATE_LEN);
	backend->parser_wrapper = backend_autodetect_parse;
	return backend;
}

static void backend_connect_handler(struct peer *peer) {
	struct backend *backend = (struct backend *) peer;

	peer_epoll_del(peer);

	int error;
  socklen_t len = sizeof(error);
	assert(getsockopt(backend->peer.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0);
	backend_connect_result(backend, error);
}

static void backend_connect_next(struct backend *backend) {
	if (backend->addr == NULL) {
		freeaddrinfo(backend->addrs);
		fprintf(stderr, "B %s: Can't connect to any addresses of %s/%s\n", backend->id, backend->node, backend->service);
		return;
	}

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(backend->addr->ai_addr, backend->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	fprintf(stderr, "B %s: Connecting to %s/%s...\n", backend->id, hbuf, sbuf);

	backend->peer.fd = socket(backend->addr->ai_family, backend->addr->ai_socktype | SOCK_NONBLOCK, backend->addr->ai_protocol);
	assert(backend->peer.fd >= 0);

	int result = connect(backend->peer.fd, backend->addr->ai_addr, backend->addr->ai_addrlen);
	backend_connect_result(backend, result == 0 ? result : errno);
}

static void backend_read(struct peer *peer) {
	struct backend *backend = (struct backend *) peer;

	if (buf_fill(&backend->buf, backend->peer.fd) <= 0) {
		fprintf(stderr, "B %s: Connection closed by backend\n", backend->id);
		close(backend->peer.fd);
		// TODO: reconnect
		return;
	}

	struct packet packet = {
		.backend = backend,
	};
	while (backend->parser_wrapper(backend, &packet)) {
		client_write(&packet);
	}

	if (backend->buf.length == BUF_LEN_MAX) {
		fprintf(stderr, "B %s: Input buffer overrun. This probably means that adsbus doesn't understand the protocol that this source is speaking.\n", backend->id);
		close(backend->peer.fd);
		// TODO: reconnect
		return;
	}
}

static void backend_connect_result(struct backend *backend, int result) {
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	assert(getnameinfo(backend->addr->ai_addr, backend->addr->ai_addrlen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0);
	switch (result) {
		case 0:
			fprintf(stderr, "B %s: Connected to %s/%s\n", backend->id, hbuf, sbuf);
			freeaddrinfo(backend->addrs);
			backend->peer.event_handler = backend_read;
			peer_epoll_add((struct peer *) backend, EPOLLIN);
			break;

		case EINPROGRESS:
			backend->peer.event_handler = backend_connect_handler;
			peer_epoll_add((struct peer *) backend, EPOLLOUT);
			break;

		default:
			fprintf(stderr, "B %s: Can't connect to %s/%s: %s\n", backend->id, hbuf, sbuf, strerror(result));
			close(backend->peer.fd);
			backend->addr = backend->addr->ai_next;
			// Tail recursion :/
			backend_connect_next(backend);
			break;
	}
}

static void backend_connect(struct backend *backend) {
	fprintf(stderr, "B %s: Resolving %s/%s...\n", backend->id, backend->node, backend->service);

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	int gai_err = getaddrinfo(backend->node, backend->service, &hints, &backend->addrs);
	if (gai_err) {
		fprintf(stderr, "B %s: Failed to resolve %s/%s: %s\n", backend->id, backend->node, backend->service, gai_strerror(gai_err));
		return;
	}
	backend->addr = backend->addrs;
	backend_connect_next(backend);
}

void backend_new(const char *node, const char *service) {
	struct backend *backend = backend_create();
	backend->node = node;
	backend->service = service;
	backend_connect(backend);
}

void backend_new_fd(int fd) {
	struct backend *backend = backend_create();
	backend->peer.fd = fd;
	backend->peer.event_handler = backend_read;
	peer_epoll_add((struct peer *) backend, EPOLLIN);

	fprintf(stderr, "B %s: New backend from incoming connection\n", backend->id);
}

void backend_new_fd_wrapper(int fd, void *unused) {
	backend_new_fd(fd);
}

void backend_print_usage() {
	fprintf(stderr, "\nSupported input formats (autodetected):\n");
	for (int i = 0; i < NUM_PARSERS; i++) {
		fprintf(stderr, "\t%s\n", parsers[i].name);
	}
}
