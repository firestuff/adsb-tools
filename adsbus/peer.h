#pragma once

#include <sys/epoll.h>

// All specific peer structs must be castable to this.
struct peer;
typedef void (*peer_event_handler)(struct peer *);
struct peer {
	int fd;
	peer_event_handler event_handler;
};
void peer_init();
void peer_epoll_add(struct peer *, uint32_t);
void peer_epoll_del(struct peer *);
void peer_loop();
