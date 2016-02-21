#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <pthread.h>

#include "common.h"

#include "wakeup.h"

static pthread_t wakeup_thread;
static int wakeup_write_fd;

static void *wakeup_main(void *arg) {
	int read_fd = (intptr_t) arg;

	int epoll_fd = epoll_create1(0);
	assert(epoll_fd >= 0);

	struct epoll_event ev = {
		.events = EPOLLIN,
	};
	assert(!epoll_ctl(epoll_fd, EPOLL_CTL_ADD, read_fd, &ev));

	while (1) {
#define MAX_EVENTS 10
		struct epoll_event events[MAX_EVENTS];
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (nfds == -1 && errno == EINTR) {
			continue;
		}
		assert(nfds >= 0);
		break; // XXX
	}

	assert(!close(read_fd));
	assert(!close(epoll_fd));
	return NULL;
}

void wakeup_init() {
	int pipefd[2];
	assert(!pipe(pipefd));
	assert(!pthread_create(&wakeup_thread, NULL, wakeup_main, (void *) (intptr_t) pipefd[0]));
	wakeup_write_fd = pipefd[1];
}

void wakeup_cleanup() {
	assert(!close(wakeup_write_fd));
	assert(!pthread_join(wakeup_thread, NULL));
}

void wakeup_add(struct peer *peer, int delay_ms) {
}
