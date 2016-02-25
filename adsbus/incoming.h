#pragma once

struct peer;

void incoming_cleanup();
typedef void (*incoming_connection_handler)(int fd, void *, struct peer *);
void incoming_new(char *, char *, incoming_connection_handler, void *);
