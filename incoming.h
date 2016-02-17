#pragma once

typedef void (*incoming_connection_handler)(int fd, void *);
void incoming_new(char *, char *, incoming_connection_handler, void *);
