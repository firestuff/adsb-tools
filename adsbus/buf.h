#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define BUF_LEN_MAX 256
struct buf {
	uint8_t buf[BUF_LEN_MAX];
	size_t start;
	size_t length;
};
#define BUF_INIT { \
	.start = 0, \
	.length = 0, \
}

#define buf_chr(buff, at) ((buff)->buf[(buff)->start + (at)])
#define buf_at(buff, at) (&buf_chr(buff, at))

void buf_init(struct buf *);
ssize_t buf_fill(struct buf *, int);
void buf_consume(struct buf *, size_t);
