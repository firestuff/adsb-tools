#pragma once

#include <stdint.h>

struct buf;
struct peer;

void exec_cleanup(void);
typedef void (*exec_connection_handler)(int fd, void *, struct peer *);
typedef void (*exec_get_hello)(struct buf **, void *);
void exec_new(char *, exec_connection_handler, exec_get_hello, void *, uint32_t *);
