#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void raw_init(void);
bool raw_parse(struct buf *, struct packet *, void *);
void raw_serialize(struct packet *, struct buf *);
