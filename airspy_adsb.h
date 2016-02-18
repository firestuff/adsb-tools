#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void airspy_adsb_init();
bool airspy_adsb_parse(struct buf *, struct packet *, void *);
void airspy_adsb_serialize(struct packet *, struct buf *);
