#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "peer.h"

#include "resolve.h"

struct resolve {
	int fd;

	char *node;
	char *service;
	int flags;

	int err;
	struct addrinfo *addrs;
};

static pthread_t resolve_thread;
static int resolve_write_fd;

static void *resolve_main(void *arg) {
	int fd = (int) (intptr_t) arg;
	struct resolve *res;
	ssize_t len;
	while ((len = read(fd, &res, sizeof(res))) == sizeof(res)) {
		struct addrinfo hints = {
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
			.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG | res->flags,
		};
		res->err = getaddrinfo(res->node, res->service, &hints, &res->addrs);
		free(res->node);
		free(res->service);
		res->node = res->service = NULL;
		assert(write(res->fd, &res, sizeof(res)) == sizeof(res));
		close(res->fd);
		res->fd = -1;
	}
	assert(!len);
	assert(!close(fd));
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

void resolve(struct peer *peer, const char *node, const char *service, int flags) {
	int fds[2];
	assert(!pipe2(fds, O_CLOEXEC));

	struct resolve *res = malloc(sizeof(*res));
	res->fd = fds[1];
	res->node = strdup(node);
	res->service = strdup(service);
	res->flags = flags;
	assert(write(resolve_write_fd, &res, sizeof(res)) == sizeof(res));

	peer->fd = fds[0];
	peer_epoll_add(peer, EPOLLIN);
}

int resolve_result(struct peer *peer, struct addrinfo **addrs) {
	struct resolve *res;
	assert(read(peer->fd, &res, sizeof(res)) == sizeof(res));
	assert(!close(peer->fd));
	peer->fd = -1;
	*addrs = res->addrs;
	int err = res->err;
	free(res);
	return err;
}
