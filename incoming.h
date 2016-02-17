#pragma once

typedef void (*incoming_connection_handler)(int fd, int epoll_fd, void *);
void incoming_new(char *, char *, int, incoming_connection_handler, void *);
