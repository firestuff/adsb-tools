#pragma once

#include <stdbool.h>
#include "backend.h"
#include "common.h"

void airspy_adsb_init();
bool airspy_adsb_parse(struct backend *, struct packet *);
