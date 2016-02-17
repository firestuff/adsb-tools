#pragma once

#include "common.h"


struct serializer *client_get_serializer(char *);
void client_add(int, struct serializer *);
void client_new_fd(int, int, void *);
void client_write(struct packet *);
