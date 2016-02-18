#pragma once

typedef void (*outgoing_connection_handler)(int fd, void *);
void outgoing_new(const char *, const char *, outgoing_connection_handler, void *);
