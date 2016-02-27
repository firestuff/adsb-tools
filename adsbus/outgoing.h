#pragma once

struct buf;
struct peer;

void outgoing_cleanup(void);
typedef void (*outgoing_connection_handler)(int fd, void *, struct peer *);
typedef void (*outgoing_get_hello)(struct buf **, void *);
void outgoing_new(char *, char *, outgoing_connection_handler, outgoing_get_hello, void *, uint32_t *);
