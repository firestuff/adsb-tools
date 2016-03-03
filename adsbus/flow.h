#pragma once

#include <stdbool.h>
#include <stdint.h>

struct buf;
struct peer;

struct flow {
	const char *name;
	void (*socket_ready)(int);
	void (*socket_connected)(int);
	void (*new)(int, void *, struct peer *);
	void (*get_hello)(struct buf **, void *);
	uint32_t *ref_count;
};

void flow_socket_ready(int, struct flow *);
void flow_socket_connected(int, struct flow *);
void flow_new(int, struct flow *, void *, struct peer *);
bool flow_new_send_hello(int, struct flow *, void *, struct peer *);
void flow_get_hello(struct flow *, struct buf **, void *);
void flow_ref_inc(struct flow *);
void flow_ref_dec(struct flow *);
