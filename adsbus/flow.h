#pragma once

#include <stdint.h>

struct buf;
struct peer;

typedef void (*flow_bound_socket_init)(int);
typedef void (*flow_new)(int, void *, struct peer *);
typedef void (*flow_get_hello)(struct buf **, void *);
struct flow {
	const char *name;
	flow_bound_socket_init bound_socket_init;
	flow_new new;
	flow_get_hello get_hello;
	uint32_t *ref_count;
};
