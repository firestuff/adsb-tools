#pragma once

#include <stdbool.h>
#include "backend.h"
#include "common.h"

void beast_init();
bool beast_parse(struct backend *, struct packet *);
