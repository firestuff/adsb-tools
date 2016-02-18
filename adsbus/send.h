#pragma once

#include "common.h"

void send_init();
struct serializer *send_get_serializer(char *);
void send_add(int, struct serializer *);
void send_add_wrapper(int, void *);
void send_write(struct packet *);
void send_print_usage();
