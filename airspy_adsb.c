#include <stdio.h>
#include <string.h>

#include "common.h"
#include "airspy_adsb.h"

bool airspy_adsb_parse(struct buf *buf, struct packet *packet) {
	if (buf->length < 1 || *buf_at(buf, 0) != '*') {
		return false;
	}
  if (buf->length >= 35 && *buf_at(buf, 34) == '\n') {
		buf_consume(buf, 35);
		return true;
	}
	if (buf->length >= 49 && *buf_at(buf, 48) == '\n') {
		buf_consume(buf, 49);
		return true;
	}
	return false;
}
