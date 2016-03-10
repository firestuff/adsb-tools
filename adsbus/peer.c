#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "log.h"
#include "server.h"

#include "peer.h"

static char log_module = 'X';

uint32_t peer_count_in = 0, peer_count_out = 0, peer_count_out_in = 0;

static int peer_epoll_fd;
static struct peer peer_shutdown_peer;
static bool peer_shutdown_flag = false;
static struct list_head peer_always_trigger_head = LIST_HEAD_INIT(peer_always_trigger_head);

static void peer_shutdown() {
	peer_close(&peer_shutdown_peer);
	peer_shutdown_flag = true;
}

static void peer_shutdown_handler(struct peer *peer) {
	struct signalfd_siginfo siginfo;
	assert(read(peer->fd, &siginfo, sizeof(siginfo)) == sizeof(siginfo));
	LOG(server_id, "Received signal %u; shutting down", siginfo.ssi_signo);
	peer_shutdown();
}

void peer_init() {
	peer_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	assert(peer_epoll_fd >= 0);

	sigset_t sigmask;
	assert(!sigemptyset(&sigmask));
	assert(!sigaddset(&sigmask, SIGINT));
	assert(!sigaddset(&sigmask, SIGTERM));
	peer_shutdown_peer.fd = signalfd(-1, &sigmask, SFD_NONBLOCK | SFD_CLOEXEC);
	assert(peer_shutdown_peer.fd >= 0);
	peer_shutdown_peer.event_handler = peer_shutdown_handler;
	peer_epoll_add(&peer_shutdown_peer, EPOLLIN);

	assert(!sigprocmask(SIG_BLOCK, &sigmask, NULL));
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
	peer->always_trigger = false;
	int res = epoll_ctl(peer_epoll_fd, EPOLL_CTL_ADD, peer->fd, &ev);
	if (res == -1 && errno == EPERM) {
		// Not a socket
		if (events) {
			list_add(&peer->peer_always_trigger_list, &peer_always_trigger_head);
			peer->always_trigger = true;
		}
	} else {
		assert(!res);
	}
}

void peer_epoll_del(struct peer *peer) {
	int res = epoll_ctl(peer_epoll_fd, EPOLL_CTL_DEL, peer->fd, NULL);
	if (res == -1 && errno == EPERM) {
		if (peer->always_trigger) {
			list_del(&peer->peer_always_trigger_list);
		}
	} else {
		assert(!res);
	}
}

void peer_close(struct peer *peer) {
	if (peer->fd == -1) {
		return;
	}
	peer_epoll_del(peer);
	assert(!close(peer->fd));
	peer->fd = -1;
}

void peer_call(struct peer *peer) {
	if (peer_shutdown_flag || !peer) {
		return;
	}
	peer->event_handler(peer);
}

void peer_loop() {
	LOG(server_id, "Starting event loop");
	while (!peer_shutdown_flag) {
		if (!(peer_count_in + peer_count_out_in)) {
			LOG(server_id, "No remaining inputs");
			peer_shutdown();
			break;
		} else if (!(peer_count_out + peer_count_out_in)) {
			LOG(server_id, "No remaining outputs");
			peer_shutdown();
			break;
		}
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int delay = list_is_empty(&peer_always_trigger_head) ? -1 : 0;
		int nfds = epoll_wait(peer_epoll_fd, events, MAX_EVENTS, delay);
		if (nfds == -1 && errno == EINTR) {
			continue;
		}
		assert(nfds >= 0);

    for (int n = 0; n < nfds; n++) {
			struct peer *peer = events[n].data.ptr;
			peer_call(peer);
		}

		{
			struct peer *iter, *next;
			list_for_each_entry_safe(iter, next, &peer_always_trigger_head, peer_always_trigger_list) {
				peer_call(iter);
			}
		}
	}
}
