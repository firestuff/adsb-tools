#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void beast_init();
bool beast_parse(struct buf *, struct packet *, void *);
void beast_serialize(struct packet *, struct buf *);