#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "common.h"
#include "uuid.h"
#include "wakeup.h"

static char server_id[UUID_LEN];
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
	uuid_gen(server_id);

	peer_epoll_fd = epoll_create1(0);
	assert(peer_epoll_fd >= 0);

	int cancel_fds[2];
	assert(!pipe2(cancel_fds, O_NONBLOCK));

	struct peer *cancel_peer = malloc(sizeof(*cancel_peer));
	assert(cancel_peer);
	cancel_peer->fd = cancel_fds[0];
	cancel_peer->event_handler = peer_cancel_handler;
	peer_epoll_add(cancel_peer, EPOLLRDHUP);

	peer_cancel_fd = cancel_fds[1];
	signal(SIGINT, peer_cancel);
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
	assert(!close(peer_epoll_fd));
}


char *packet_type_names[] = {
	"Mode-S short",
	"Mode-S long",
};


static uint64_t mlat_timestamp_scale_mhz_in(uint64_t timestamp, uint32_t mhz) {
	return timestamp * (MLAT_MHZ / mhz);
}

static uint64_t mlat_timestamp_scale_width_in(uint64_t timestamp, uint64_t max, struct mlat_state *state) {
	if (timestamp < state->timestamp_last) {
		// Counter reset
		state->timestamp_generation += max;
	}

	state->timestamp_last = timestamp;
	return state->timestamp_generation + timestamp;
}

uint64_t mlat_timestamp_scale_in(uint64_t timestamp, uint64_t max, uint16_t mhz, struct mlat_state *state) {
	return mlat_timestamp_scale_mhz_in(mlat_timestamp_scale_width_in(timestamp, max, state), mhz);
}

static uint64_t mlat_timestamp_scale_mhz_out(uint64_t timestamp, uint64_t mhz) {
	return timestamp / (MLAT_MHZ / mhz);
}

static uint64_t mlat_timestamp_scale_width_out(uint64_t timestamp, uint64_t max) {
	return timestamp % max;
}

uint64_t mlat_timestamp_scale_out(uint64_t timestamp, uint64_t max, uint16_t mhz) {
	return mlat_timestamp_scale_width_out(mlat_timestamp_scale_mhz_out(timestamp, mhz), max);
}


uint32_t rssi_scale_in(uint32_t value, uint32_t max) {
	return value * (RSSI_MAX / max);
}

uint32_t rssi_scale_out(uint32_t value, uint32_t max) {
	return value / (RSSI_MAX / max);
}
