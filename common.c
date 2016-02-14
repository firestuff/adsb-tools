#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"


void buf_init(struct buf *buf, char *main, char *tmp) {
	buf->buf = main;
	buf->tmp = tmp;
	buf->start = 0;
	buf->length = 0;
}

void buf_alias(struct buf *to, struct buf *from) {
	memcpy(to, from, sizeof(*to));
}

ssize_t buf_fill(struct buf *buf, int fd) {
	if (buf->start + buf->length == BUF_LEN) {
		assert(buf->start > 0);
		memmove(buf->buf, buf_at(buf, 0), buf->length);
		buf->start = 0;
	}

	size_t space = BUF_LEN - buf->length - buf->start;
	ssize_t in = read(fd, buf_at(buf, buf->length), space);
	if (in < 0) {
		return in;
	}
	buf->length += in;
	return in;
}

void buf_consume(struct buf *buf, size_t length) {
	assert(buf->length >= length);

	buf->length -= length;
	if (buf->length) {
		buf->start += length;
	} else {
		buf->start = 0;
	}
}
