#pragma once

#include <sys/epoll.h>

// All specific peer structs must be castable to this.
struct peer;
typedef void (*peer_event_handler)(struct peer *);
struct peer {
	int fd;
	peer_event_handler event_handler;
};

extern uint32_t peer_count_in, peer_count_out;

void peer_init();
void peer_cleanup();
void peer_shutdown();
void peer_epoll_add(struct peer *, uint32_t);
void peer_epoll_del(struct peer *);
void peer_call(struct peer *);
void peer_loop();
