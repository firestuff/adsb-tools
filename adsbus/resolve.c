#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer.h"

#include "resolve.h"

struct resolve {
	struct peer peer;

	struct addrinfo **addrs;
	const char **error;

	struct peer *inner_peer;

	struct addrinfo ar_request;
	struct gaicb gaicb;
	struct gaicb *requests[1];

	struct sigevent sev;
};

static void resolve_handler(struct peer *peer) {
	struct resolve *res = (struct resolve *) peer;

	assert(!close(res->peer.fd));

	int err = gai_error(&res->gaicb);
	if (err == 0) {
		*res->addrs = res->gaicb.ar_result;
		*res->error = NULL;
	} else {
		*res->addrs = NULL;
		*res->error = gai_strerror(err);
	}
	struct peer *inner_peer = res->inner_peer;
	free(res);
	inner_peer->event_handler(inner_peer);
}

static void resolve_callback(union sigval val) {
	assert(!close(val.sival_int));
}

void resolve(struct peer *peer, const char *node, const char *service, int flags, struct addrinfo **addrs, const char **error) {
	struct resolve *res = malloc(sizeof(*res));
	res->addrs = addrs;
	res->error = error;
	res->inner_peer = peer;

	int fds[2];
	assert(!pipe2(fds, O_NONBLOCK));
	res->peer.fd = fds[0];
	res->peer.event_handler = resolve_handler;
	peer_epoll_add((struct peer *) res, EPOLLRDHUP);

	res->requests[0] = &res->gaicb;
	res->gaicb.ar_name = node;
	res->gaicb.ar_service = service;
	res->gaicb.ar_request = &res->ar_request;
	res->ar_request.ai_family = AF_UNSPEC;
	res->ar_request.ai_socktype = SOCK_STREAM;
	res->ar_request.ai_protocol = 0;
	res->ar_request.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | flags;

	res->sev.sigev_notify = SIGEV_THREAD;
	res->sev.sigev_value.sival_int = fds[1];
	res->sev.sigev_notify_function = resolve_callback;
	res->sev.sigev_notify_attributes = NULL;

	assert(!getaddrinfo_a(GAI_NOWAIT, res->requests, 1, &res->sev));
}
