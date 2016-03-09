#include <netdb.h>

#include "asyncaddrinfo.h"
#include "peer.h"

#include "resolve.h"

void resolve_init() {
	asyncaddrinfo_init(2);
}

void resolve_cleanup() {
	asyncaddrinfo_cleanup();
}

void resolve(struct peer *peer, const char *node, const char *service, int flags) {
	struct addrinfo hints = {
		.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | flags,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	peer->fd = asyncaddrinfo_resolve(node, service, &hints);
	peer_epoll_add(peer, EPOLLIN);
}

int resolve_result(struct peer *peer, struct addrinfo **addrs) {
	int err = asyncaddrinfo_result(peer->fd, addrs);
	peer->fd = -1;
	return err;
}
