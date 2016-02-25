#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "server.h"
#include "uuid.h"
#include "wakeup.h"

#include "peer.h"

static int peer_epoll_fd;
static int peer_shutdown_fd;
static bool peer_shutdown_flag = false;

static void peer_shutdown_handler(struct peer *peer) {
	fprintf(stderr, "X %s: Shutting down\n", server_id);
	assert(!close(peer->fd));
	free(peer);
	peer_shutdown_flag = true;
}

void peer_init() {
	peer_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	assert(peer_epoll_fd >= 0);

	int shutdown_fds[2];
	assert(!pipe2(shutdown_fds, O_CLOEXEC));

	struct peer *shutdown_peer = malloc(sizeof(*shutdown_peer));
	assert(shutdown_peer);
	shutdown_peer->fd = shutdown_fds[0];
	shutdown_peer->event_handler = peer_shutdown_handler;
	peer_epoll_add(shutdown_peer, EPOLLRDHUP);

	peer_shutdown_fd = shutdown_fds[1];
	signal(SIGINT, peer_shutdown);
}

void peer_cleanup() {
	assert(!close(peer_epoll_fd));
}

void peer_shutdown(int signal) {
	assert(!close(peer_shutdown_fd));
}

void peer_epoll_add(struct peer *peer, uint32_t events) {
	struct epoll_event ev = {
		.events = events,
		.data = {
			.ptr = peer,
		},
	};
	assert(!epoll_ctl(peer_epoll_fd, EPOLL_CTL_ADD, peer->fd, &ev));
}

void peer_epoll_del(struct peer *peer) {
	assert(!epoll_ctl(peer_epoll_fd, EPOLL_CTL_DEL, peer->fd, NULL));
}

void peer_call(struct peer *peer) {
	if (peer_shutdown_flag || !peer) {
		return;
	}
	peer->event_handler(peer);
}

void peer_loop() {
	fprintf(stderr, "X %s: Starting event loop\n", server_id);
	while (!peer_shutdown_flag) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(peer_epoll_fd, events, MAX_EVENTS, wakeup_get_delay());
		if (nfds == -1 && errno == EINTR) {
			continue;
		}
		assert(nfds >= 0);

    for (int n = 0; n < nfds; n++) {
			struct peer *peer = events[n].data.ptr;
			peer->event_handler(peer);
		}

		wakeup_dispatch();
	}
}
