#pragma once

#include "common.h"


void json_init();
size_t json_serialize(struct packet *, char *);
