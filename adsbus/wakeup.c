#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "list.h"
#include "peer.h"
#include "rand.h"

#include "wakeup.h"

struct wakeup_entry {
	int fd;
	uint64_t absolute_time_ms;
	struct peer *peer;
	struct list_head wakeup_list;
};

static struct list_head wakeup_head = LIST_HEAD_INIT(wakeup_head);

static uint64_t wakeup_get_time_ms() {
	struct timespec tp;
	assert(!clock_gettime(CLOCK_MONOTONIC_COARSE, &tp));
#define MS_PER_S UINT64_C(1000)
#define NS_PER_MS UINT64_C(1000000)
	assert(tp.tv_sec >= 0);
	assert(tp.tv_nsec >= 0);
	return ((uint64_t) tp.tv_sec * MS_PER_S) + ((uint64_t) tp.tv_nsec / NS_PER_MS);
}

void wakeup_init() {
}

void wakeup_cleanup() {
	struct wakeup_entry *iter, *next;
	list_for_each_entry_safe(iter, next, &wakeup_head, wakeup_list) {
		free(iter);
	}
}

int wakeup_get_delay() {
	if (list_is_empty(&wakeup_head)) {
		return -1;
	}
	uint64_t now = wakeup_get_time_ms();
	struct wakeup_entry *next_to_fire = list_entry(wakeup_head.next, struct wakeup_entry, wakeup_list);
	if (next_to_fire->absolute_time_ms > now) {
		uint64_t delta = next_to_fire->absolute_time_ms - now;
		assert(delta < INT_MAX);
		return (int) delta;
	} else {
		return 0;
	}
}

void wakeup_dispatch() {
	uint64_t now = wakeup_get_time_ms();
	struct wakeup_entry *iter, *next;
	list_for_each_entry_safe(iter, next, &wakeup_head, wakeup_list) {
		if (iter->absolute_time_ms > now) {
			break;
		}
		peer_call(iter->peer);
		list_del(&iter->wakeup_list);
		free(iter);
	}
}

void wakeup_add(struct peer *peer, uint32_t delay_ms) {
	struct wakeup_entry *entry = malloc(sizeof(*entry));
	entry->absolute_time_ms = wakeup_get_time_ms() + delay_ms;
	entry->peer = peer;

	struct wakeup_entry *iter, *next;
	list_for_each_entry_safe(iter, next, &wakeup_head, wakeup_list) {
		if (iter->absolute_time_ms > entry->absolute_time_ms) {
			list_add(&entry->wakeup_list, &iter->wakeup_list);
			return;
		}
	}
	list_add(&entry->wakeup_list, &wakeup_head);
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
