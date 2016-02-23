#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/epoll.h>

#include "common.h"
#include "peer.h"
#include "rand.h"

#include "wakeup.h"

struct wakeup_entry {
	int fd;
	uint64_t absolute_time_ms;
	struct peer *peer;
	struct wakeup_entry *next;
};

struct wakeup_entry *head = NULL;

static uint64_t wakeup_get_time_ms() {
	struct timespec tp;
	assert(!clock_gettime(CLOCK_MONOTONIC_COARSE, &tp));
#define MS_PER_S 1000
#define NS_PER_MS 1000000
	return (tp.tv_sec * MS_PER_S) + (tp.tv_nsec / NS_PER_MS);
}

void wakeup_init() {
}

void wakeup_cleanup() {
	while (head) {
		struct wakeup_entry *next = head->next;
		free(head);
		head = next;
	}
}

int wakeup_get_delay() {
	if (!head) {
		return -1;
	}
	int delay = head->absolute_time_ms - wakeup_get_time_ms();
	return delay < 0 ? 0 : delay;
}

void wakeup_dispatch() {
	uint64_t now = wakeup_get_time_ms();
	while (head && head->absolute_time_ms <= now) {
		struct peer *peer = head->peer;
		struct wakeup_entry *next = head->next;
		free(head);
		head = next;
		peer->event_handler(peer);
	}
}

void wakeup_add(struct peer *peer, uint32_t delay_ms) {
	struct wakeup_entry *entry = malloc(sizeof(*entry));
	entry->absolute_time_ms = wakeup_get_time_ms() + delay_ms;
	entry->peer = peer;

	struct wakeup_entry *prev = NULL, *iter = head;
	while (iter) {
		if (iter->absolute_time_ms > entry->absolute_time_ms) {
			break;
		}
		prev = iter;
		iter = iter->next;
	}

	if (prev) {
		entry->next = prev->next;
		prev->next = entry;
	} else {
		entry->next = head;
		head = entry;
	}
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
