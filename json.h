#pragma once

#include "common.h"


void json_init();
void json_serialize(struct packet *, struct buf *);

int json_buf_append_callback(const char *, size_t, void *);
