#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "asyncaddrinfo.h"

struct asyncaddrinfo_resolution {
	int return_fd;

	char *node;
	char *service;
	struct addrinfo _hints, *hints;

	int err;
	struct addrinfo *addrs;
};

static size_t asyncaddrinfo_num_threads;
static pthread_t *asyncaddrinfo_threads = NULL;
static int asyncaddrinfo_write_fd;

static void *asyncaddrinfo_main(void *arg) {
	int fd = (int) (intptr_t) arg;
	struct asyncaddrinfo_resolution *res;
	ssize_t len;
	while ((len = recv(fd, &res, sizeof(res), 0)) == sizeof(res)) {
		res->err = getaddrinfo(res->node, res->service, res->hints, &res->addrs);
		int return_fd = res->return_fd;
		res->return_fd = -1;
		assert(send(return_fd, &res, sizeof(res), MSG_EOR) == sizeof(res));
		// Main thread now owns res
		assert(!close(return_fd));
	}
	assert(!len);
	assert(!close(fd));
	return NULL;
}

static void asyncaddrinfo_del(struct asyncaddrinfo_resolution *res) {
	if (res->node) {
		free(res->node);
		res->node = NULL;
	}
	if (res->service) {
		free(res->service);
		res->service = NULL;
	}
	free(res);
}

void asyncaddrinfo_init(size_t threads) {
	assert(!asyncaddrinfo_threads);

	int fds[2];
	assert(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds));
	asyncaddrinfo_write_fd = fds[1];

	asyncaddrinfo_num_threads = threads;
	asyncaddrinfo_threads = malloc(asyncaddrinfo_num_threads * sizeof(*asyncaddrinfo_threads));
	assert(asyncaddrinfo_threads);

	for (size_t i = 0; i < asyncaddrinfo_num_threads; i++) {
		int subfd = fcntl(fds[0], F_DUPFD_CLOEXEC, 0);
		assert(subfd >= 0);
		assert(!pthread_create(&asyncaddrinfo_threads[i], NULL, asyncaddrinfo_main, (void *) (intptr_t) subfd));
	}
	assert(!close(fds[0]));
}

void asyncaddrinfo_cleanup() {
	assert(asyncaddrinfo_threads);
	assert(!close(asyncaddrinfo_write_fd));
	asyncaddrinfo_write_fd = -1;
	for (size_t i = 0; i < asyncaddrinfo_num_threads; i++) {
		assert(!pthread_join(asyncaddrinfo_threads[i], NULL));
	}
	free(asyncaddrinfo_threads);
	asyncaddrinfo_threads = NULL;
}

int asyncaddrinfo_resolve(const char *node, const char *service, const struct addrinfo *hints) {
	int fds[2];
	assert(!socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, fds));

	struct asyncaddrinfo_resolution *res = malloc(sizeof(*res));
	assert(res);
	res->return_fd = fds[1];
	if (node) {
		res->node = strdup(node);
		assert(res->node);
	} else {
		res->node = NULL;
	}
	if (service) {
		res->service = strdup(service);
		assert(res->service);
	} else {
		res->service = NULL;
	}
	if (hints) {
		memcpy(&res->_hints, hints, sizeof(res->_hints));
		res->hints = &res->_hints;
	} else {
		res->hints = NULL;
	}
	assert(send(asyncaddrinfo_write_fd, &res, sizeof(res), MSG_EOR) == sizeof(res));
	// Resolve thread now owns res

	return fds[0];
}

int asyncaddrinfo_result(int fd, struct addrinfo **addrs) {
	struct asyncaddrinfo_resolution *res;
	assert(recv(fd, &res, sizeof(res), 0) == sizeof(res));
	assert(!close(fd));
	*addrs = res->addrs;
	int err = res->err;
	asyncaddrinfo_del(res);
	return err;
}
