#pragma once

#include <stdbool.h>
#include "backend.h"
#include "common.h"

void raw_init();
bool raw_parse(struct backend *, struct packet *);
