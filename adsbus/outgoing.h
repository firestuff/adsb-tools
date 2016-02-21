#pragma once

void outgoing_cleanup();
typedef void (*outgoing_connection_handler)(int fd, void *);
void outgoing_new(char *, char *, outgoing_connection_handler, void *);
