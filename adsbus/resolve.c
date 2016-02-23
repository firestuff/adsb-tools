#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "peer.h"

#include "resolve.h"

struct resolve_request {
	int fd;
	const char *node;
	const char *service;
	int flags;
	struct addrinfo **addrs;
	const char **error;
};

struct resolve_peer {
	struct peer peer;
	struct peer *inner_peer;
};

static pthread_t resolve_thread;
static int resolve_write_fd;

static void resolve_handler(struct peer *peer) {
	struct resolve_peer *resolve_peer = (struct resolve_peer *) peer;

	assert(!close(resolve_peer->peer.fd));

	struct peer *inner_peer = resolve_peer->inner_peer;
	free(resolve_peer);
	inner_peer->event_handler(inner_peer);
}

static void *resolve_main(void *arg) {
	int fd = (intptr_t) arg;
	struct resolve_request *request;
	ssize_t ret;
	while ((ret = read(fd, &request, sizeof(request))) == sizeof(request)) {
		struct addrinfo hints = {
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
			.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | request->flags,
		};
		int err = getaddrinfo(request->node, request->service, &hints, request->addrs);
		if (err) {
			*request->addrs = NULL;
			*request->error = gai_strerror(err);
		} else {
			*request->error = NULL;
		}
		close(request->fd);
		free(request);
	}
	assert(!ret);
	return NULL;
}

void resolve_init() {
	int fds[2];
	assert(!pipe2(fds, O_CLOEXEC));
	resolve_write_fd = fds[1];
	assert(!pthread_create(&resolve_thread, NULL, resolve_main, (void *) (intptr_t) fds[0]));
}

void resolve_cleanup() {
	assert(!close(resolve_write_fd));
	assert(!pthread_join(resolve_thread, NULL));
}

void resolve(struct peer *peer, const char *node, const char *service, int flags, struct addrinfo **addrs, const char **error) {
	int fds[2];
	assert(!pipe2(fds, O_CLOEXEC));

	struct resolve_request *request = malloc(sizeof(*request));
	request->fd = fds[1];
	request->node = node;
	request->service = service;
	request->flags = flags;
	request->addrs = addrs;
	request->error = error;
	assert(write(resolve_write_fd, &request, sizeof(request)) == sizeof(request));

	struct resolve_peer *resolve_peer = malloc(sizeof(*resolve_peer));
	resolve_peer->peer.fd = fds[0];
	resolve_peer->peer.event_handler = resolve_handler;
	resolve_peer->inner_peer = peer;
	peer_epoll_add((struct peer *) resolve_peer, EPOLLRDHUP);
}
