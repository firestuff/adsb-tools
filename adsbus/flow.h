#pragma once

#include <stdbool.h>
#include <stdint.h>

struct buf;
struct peer;

struct flow {
	const char *name;
	void (*socket_connected)(int);
	void (*new)(int, void *, struct peer *);
	void (*get_hello)(struct buf **, void *);
	uint32_t *ref_count;
};

void flow_socket_connected(int, struct flow *);
bool flow_hello(int, struct flow *, void *);
