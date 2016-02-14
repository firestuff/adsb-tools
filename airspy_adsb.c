#include <stdio.h>
#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

bool airspy_adsb_parse(struct buf *buf, struct packet *packet) {
	if (buf->length < 35 ||
	    buf_chr(buf, 0) != '*') {
		return false;
	}
	if (buf->length >= 35 &&
	    buf_chr(buf, 34) == '\n' &&
			buf_chr(buf, 15) == ';') {
		packet->type = MODE_S_SHORT;
		buf_consume(buf, 35);
		return true;
	}
	if (buf->length >= 49 &&
	    buf_chr(buf, 48) == '\n' &&
			buf_chr(buf, 29) == ';') {
		packet->type = MODE_S_LONG;
		buf_consume(buf, 49);
		return true;
	}
	return false;
}
