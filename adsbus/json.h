#pragma once

#include <stdbool.h>

struct buf;
struct packet;

void json_init(void);
void json_cleanup(void);
bool __attribute__ ((warn_unused_result)) json_parse(struct buf *, struct packet *, void *);
void json_serialize(struct packet *, struct buf *);

int json_buf_append_callback(const char *, size_t, void *);
