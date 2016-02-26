#pragma once

#include <stdint.h>

struct peer;

void incoming_cleanup(void);
typedef void (*incoming_connection_handler)(int fd, void *, struct peer *);
void incoming_new(char *, char *, incoming_connection_handler, void *, uint32_t *);
