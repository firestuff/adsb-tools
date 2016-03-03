#include <assert.h>
#include <unistd.h>

#include "buf.h"
#include "socket.h"

#include "flow.h"

static bool flow_send_hello(int fd, struct flow *flow, void *passthrough) {
	struct buf buf = BUF_INIT, *buf_ptr = &buf;
	flow_get_hello(flow, &buf_ptr, passthrough);
	if (!buf_ptr->length) {
		return true;
	}
	return (write(fd, buf_at(buf_ptr, 0), buf_ptr->length) == (ssize_t) buf_ptr->length);
}

void flow_socket_ready(int fd, struct flow *flow) {
	socket_ready(fd);
	if (flow->socket_ready) {
		flow->socket_ready(fd);
	}
}

void flow_socket_connected(int fd, struct flow *flow) {
	if (flow->socket_connected) {
		flow->socket_connected(fd);
	}
}

void flow_new(int fd, struct flow *flow, void *passthrough, struct peer *on_close) {
	flow->new(fd, passthrough, on_close);
}

bool flow_new_send_hello(int fd, struct flow *flow, void *passthrough, struct peer *on_close) {
	if (!flow_send_hello(fd, flow, passthrough)) {
		assert(!close(fd));
		return false;
	}
	flow_new(fd, flow, passthrough, on_close);
	return true;
}

void flow_get_hello(struct flow *flow, struct buf **buf_ptr, void *passthrough) {
	if (!flow->get_hello) {
		return;
	}
	flow->get_hello(buf_ptr, passthrough);
}

void flow_ref_inc(struct flow *flow) {
	(*flow->ref_count)++;
}

void flow_ref_dec(struct flow *flow) {
	(*flow->ref_count)--;
}
