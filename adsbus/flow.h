#pragma once

#include <stdbool.h>
#include <stdint.h>

struct buf;
struct peer;

struct flow {
	const char *name;
	void (*socket_ready)(int);
	void (*new)(int, void *, struct peer *);
	void (*get_hello)(struct buf **, void *);
	uint32_t *ref_count;
};

void flow_socket_ready(int, struct flow *);
bool flow_hello(int, struct flow *, void *);
bool flow_new(int, struct flow *, void *);
void flow_ref_inc(struct flow *);
void flow_ref_dec(struct flow *);
