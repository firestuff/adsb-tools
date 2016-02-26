#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "socket.h"

void socket_init(int fd) {
	int optval = 1;
	int err = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
	if (err == -1 && errno == ENOTSOCK) {
		return;
	}
	assert(!err);

	optval = 30;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)));
	optval = 10;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)));
	optval = 3;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval)));
}
