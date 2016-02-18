#pragma once

#include "common.h"

void client_init();
struct serializer *client_get_serializer(char *);
void client_add(int, struct serializer *);
void client_add_wrapper(int, void *);
void client_write(struct packet *);
void client_print_usage();
