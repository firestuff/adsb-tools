#pragma once

struct peer;

void outgoing_cleanup();
typedef void (*outgoing_connection_handler)(int fd, void *, struct peer *);
void outgoing_new(char *, char *, outgoing_connection_handler, void *);
