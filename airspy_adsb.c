#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

bool airspy_adsb_parse(struct buf *buf, struct packet *packet) {
	if (buf->length < 35) {
		// Minimum frame length
		return false;
	}
	if (buf->buf[buf->start] != '*') {
		return false;
	}
	char *last = memchr(&buf->buf[buf->start], '\n', buf->length);
	if (!last) {
		return false;
	}
	buf_consume(buf, last - &buf->buf[buf->start] + 1);
	return true;
}
