#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void airspy_adsb_init(void);
bool __attribute__ ((warn_unused_result)) airspy_adsb_parse(struct buf *, struct packet *, void *);
void airspy_adsb_serialize(struct packet *, struct buf *);
