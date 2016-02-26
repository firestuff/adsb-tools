#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void json_init(void);
void json_cleanup(void);
bool json_parse(struct buf *, struct packet *, void *);
void json_serialize(struct packet *, struct buf *);

int json_buf_append_callback(const char *, size_t, void *);
