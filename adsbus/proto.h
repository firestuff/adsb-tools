#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void proto_cleanup(void);
bool __attribute__ ((warn_unused_result)) proto_parse(struct buf *, struct packet *, void *);
void proto_serialize(struct packet *, struct buf *);
