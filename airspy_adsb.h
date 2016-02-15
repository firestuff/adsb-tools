#include <stdbool.h>

void airspy_adsb_init();
bool airspy_adsb_parse(struct buf *, struct packet *, void *);
