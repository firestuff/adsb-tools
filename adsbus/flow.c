#include <unistd.h>

#include "buf.h"
#include "socket.h"

#include "flow.h"

void flow_socket_connected(int fd, struct flow *flow) {
	socket_connected(fd);
	if (flow->socket_connected) {
		flow->socket_connected(fd);
	}
}

bool flow_hello(int fd, struct flow *flow, void *passthrough) {
	if (!flow->get_hello) {
		return true;
	}
	struct buf buf = BUF_INIT, *buf_ptr = &buf;
	flow->get_hello(&buf_ptr, passthrough);
	if (!buf_ptr->length) {
		return true;
	}
	return (write(fd, buf_at(buf_ptr, 0), buf_ptr->length) == (ssize_t) buf_ptr->length);
}

bool flow_new(int fd, struct flow *flow, void *passthrough) {
	if (!flow_hello(fd, flow, passthrough)) {
		return false;
	}
	flow->new(fd, passthrough, NULL);
	return true;
}
