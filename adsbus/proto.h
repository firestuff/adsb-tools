#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void proto_cleanup(void);
bool proto_parse(struct buf *, struct packet *, void *);
void proto_serialize(struct packet *, struct buf *);
