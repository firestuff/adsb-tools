#pragma once

#include <stdint.h>

struct buf;
struct peer;

void incoming_cleanup(void);
typedef void (*incoming_connection_handler)(int fd, void *, struct peer *);
typedef void (*incoming_get_hello)(struct buf **, void *);
void incoming_new(char *, char *, incoming_connection_handler, incoming_get_hello, void *, uint32_t *);
