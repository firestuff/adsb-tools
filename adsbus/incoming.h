#pragma once

typedef void (*incoming_connection_handler)(int fd, void *);
void incoming_new(const char *, const char *, incoming_connection_handler, void *);
