#include <unistd.h>

#include "buf.h"

#include "flow.h"

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
