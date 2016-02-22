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
#include <pthread.h>

#include "common.h"

#include "wakeup.h"

struct wakeup_peer {
	struct peer peer;
	struct peer *inner_peer;
};

struct wakeup_request {
	int fd;
	uint64_t absolute_time_ms;
};

struct wakeup_entry {
	struct wakeup_request request;
	struct wakeup_entry *next;
};

static pthread_t wakeup_thread;
static int wakeup_write_fd;

static uint64_t wakeup_get_time_ms() {
	struct timespec tp;
	assert(!clock_gettime(CLOCK_MONOTONIC_COARSE, &tp));
#define MS_PER_S 1000
#define NS_PER_MS 1000000
	return (tp.tv_sec * MS_PER_S) + (tp.tv_nsec / NS_PER_MS);
}

static void wakeup_request_add(struct wakeup_entry **head, struct wakeup_request *request) {
	struct wakeup_entry *entry = malloc(sizeof(*entry));
	memcpy(&entry->request, request, sizeof(entry->request));

	struct wakeup_entry *prev = NULL, *iter = *head;
	while (iter) {
		if (iter->request.absolute_time_ms > entry->request.absolute_time_ms) {
			break;
		}
		prev = iter;
		iter = iter->next;
	}

	if (prev) {
		entry->next = prev->next;
		prev->next = entry;
	} else {
		entry->next = *head;
		*head = entry;
	}
}

static void *wakeup_main(void *arg) {
	int read_fd = (intptr_t) arg;

	int epoll_fd = epoll_create1(0);
	assert(epoll_fd >= 0);

	struct epoll_event ev = {
		.events = EPOLLIN,
		.data = {
			.fd = read_fd,
		},
	};
	assert(!epoll_ctl(epoll_fd, EPOLL_CTL_ADD, read_fd, &ev));

	struct wakeup_entry *head = NULL;

	while (1) {
		int delay = -1;
		if (head) {
			// This call is the whole reason we need a separate thread: we want to avoid getting the time inside the tight main thread loop.
			delay = head->request.absolute_time_ms - wakeup_get_time_ms();
			delay = delay < 0 ? 0 : delay;
		}

#define MAX_EVENTS 1
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, delay);
		if (nfds == -1 && errno == EINTR) {
			continue;
		}

		if (nfds == 1) {
			assert(events[0].data.fd == read_fd);
			struct wakeup_request request;
			ssize_t result = read(read_fd, &request, sizeof(request));
			if (result == 0) {
				// Peer closed connection, shutdown thread
				break;
			}
			assert(result == sizeof(request));
			wakeup_request_add(&head, &request);
		} else {
			assert(nfds == 0);
		}

		uint64_t now = wakeup_get_time_ms();
		while (head && head->request.absolute_time_ms <= now) {
			close(head->request.fd);
			struct wakeup_entry *next = head->next;
			free(head);
			head = next;
		}
	}

	while (head) {
		close(head->request.fd);
		struct wakeup_entry *next = head->next;
		free(head);
		head = next;
	}

	assert(!close(read_fd));
	assert(!close(epoll_fd));
	return NULL;
}

static void wakeup_handler(struct peer *peer) {
	struct wakeup_peer *outer_peer = (struct wakeup_peer *) peer;
	assert(!close(outer_peer->peer.fd));

	struct peer *inner_peer = outer_peer->inner_peer;
	free(outer_peer);
	inner_peer->event_handler(inner_peer);
}

void wakeup_init() {
	int pipefd[2];
	assert(!pipe2(pipefd, O_NONBLOCK));
	assert(!pthread_create(&wakeup_thread, NULL, wakeup_main, (void *) (intptr_t) pipefd[0]));
	wakeup_write_fd = pipefd[1];
}

void wakeup_cleanup() {
	assert(!close(wakeup_write_fd));
	assert(!pthread_join(wakeup_thread, NULL));
}

void wakeup_add(struct peer *peer, uint32_t delay_ms) {
	int pipefd[2];
	assert(!pipe2(pipefd, O_NONBLOCK));

	struct wakeup_request request = { 0 };
	request.fd = pipefd[1],
	request.absolute_time_ms = wakeup_get_time_ms() + delay_ms;
	assert(write(wakeup_write_fd, &request, sizeof(request)) == sizeof(request));

	struct wakeup_peer *outer_peer = malloc(sizeof(*outer_peer));
	assert(outer_peer);
	outer_peer->peer.fd = pipefd[0];
	outer_peer->peer.event_handler = wakeup_handler;
	outer_peer->inner_peer = peer;

	peer_epoll_add((struct peer *) outer_peer, EPOLLRDHUP);
}
