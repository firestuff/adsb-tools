#pragma once

struct packet;
struct buf;

void stats_init();
void stats_serialize(struct packet *, struct buf *);
