#include "buf.h"
#include "packet.h"

#include "proto.h"

bool proto_parse(struct buf *buf, struct packet *packet, void *state_in) {
	return false;
}

void proto_serialize(struct packet *packet, struct buf *buf) {
}
