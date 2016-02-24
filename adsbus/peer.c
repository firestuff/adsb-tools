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
static int peer_cancel_fd;
static bool peer_canceled = false;

static void peer_cancel(int signal) {
	assert(!close(peer_cancel_fd));
}

static void peer_cancel_handler(struct peer *peer) {
	fprintf(stderr, "X %s: Shutting down\n", server_id);
	assert(!close(peer->fd));
	free(peer);
	peer_canceled = true;
}

void peer_init() {
	peer_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	assert(peer_epoll_fd >= 0);

	int cancel_fds[2];
	assert(!pipe2(cancel_fds, O_CLOEXEC));

	struct peer *cancel_peer = malloc(sizeof(*cancel_peer));
	assert(cancel_peer);
	cancel_peer->fd = cancel_fds[0];
	cancel_peer->event_handler = peer_cancel_handler;
	peer_epoll_add(cancel_peer, EPOLLRDHUP);

	peer_cancel_fd = cancel_fds[1];
	signal(SIGINT, peer_cancel);
}

void peer_cleanup() {
	assert(!close(peer_epoll_fd));
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

void peer_loop() {
	fprintf(stderr, "X %s: Starting event loop\n", server_id);
	while (!peer_canceled) {
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
