#pragma once

#include <sys/epoll.h>

#include "list.h"

// All specific peer structs must be castable to this.
struct peer;
typedef void (*peer_event_handler)(struct peer *);
struct peer {
	int fd;
	peer_event_handler event_handler;
	struct list_head peer_always_trigger_list;
	bool always_trigger;
};

extern uint32_t peer_count_in, peer_count_out, peer_count_out_in;

void peer_init(void);
void peer_cleanup(void);
void peer_shutdown(int signal);
void peer_epoll_add(struct peer *, uint32_t);
void peer_epoll_del(struct peer *);
void peer_call(struct peer *);
void peer_loop(void);
