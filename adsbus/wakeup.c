#include <assert.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "peer.h"
#include "rand.h"

#include "wakeup.h"

struct wakeup {
	struct peer peer;
	struct peer *inner_peer;
	struct list_head wakeup_list;
};

static struct list_head wakeup_head = LIST_HEAD_INIT(wakeup_head);

static void wakeup_handler(struct peer *peer) {
	struct wakeup *wakeup = container_of(peer, struct wakeup, peer);

	uint64_t events;
	assert(read(wakeup->peer.fd, &events, sizeof(events)) == sizeof(events));
	assert(events == 1);

	peer_close(&wakeup->peer);
	peer_call(wakeup->inner_peer);
	list_del(&wakeup->wakeup_list);
	free(wakeup);
}

void wakeup_init() {
}

void wakeup_cleanup() {
	struct wakeup *iter, *next;
	list_for_each_entry_safe(iter, next, &wakeup_head, wakeup_list) {
		free(iter);
	}
}

void wakeup_add(struct peer *peer, uint32_t delay_ms) {
	struct wakeup *wakeup = malloc(sizeof(*wakeup));
	wakeup->inner_peer = peer;
	list_add(&wakeup->wakeup_list, &wakeup_head);

	wakeup->peer.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	assert(wakeup->peer.fd >= 0);

#define MS_PER_S UINT64_C(1000)
#define NS_PER_MS UINT64_C(1000000)
	const struct itimerspec ts = {
		.it_interval = {
			.tv_sec = 0,
			.tv_nsec = 0,
		},
		.it_value = {
			.tv_sec = delay_ms / MS_PER_S,
			.tv_nsec = (delay_ms % MS_PER_S) * NS_PER_MS,
		},
	};
	assert(!timerfd_settime(wakeup->peer.fd, 0, &ts, NULL));

	wakeup->peer.event_handler = wakeup_handler;
	peer_epoll_add(&wakeup->peer, EPOLLIN);
}

#define RETRY_MIN_MS 2000
#define RETRY_MAX_MS 60000
uint32_t wakeup_get_retry_delay_ms(uint32_t attempt) {
	uint32_t max_delay = RETRY_MIN_MS * (1 << attempt);
	max_delay = max_delay > RETRY_MAX_MS ? RETRY_MAX_MS : max_delay;

	uint32_t jitter;
	rand_fill(&jitter, sizeof(jitter));

	return jitter % max_delay;
}
