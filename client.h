#pragma once

#include "common.h"

#define SERIALIZE_LEN 256


struct serializer *client_get_serializer(char *);
void client_add(int, struct serializer *);
void client_write(struct packet *);
