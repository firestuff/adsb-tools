#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "socket.h"

void socket_pre_bind_init(int fd) {
	// Called by transport code; safe to assume that fd is a socket
	int optval = 1;
	assert(!setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)));
}

void socket_bound_init(int fd) {
	// Called by transport code; safe to assume that fd is a socket
	int qlen = 5;
	assert(!setsockopt(fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen)));

	// These options are inherited through accept()
	// Verified by flamingcow on kernel 4.2.0
	socket_connected_init(fd);
}

void socket_connected_init(int fd) {
	// Called by transport code; safe to assume that fd is a socket
	int optval = 1;
	assert(!setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)));
	optval = 30;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, sizeof(optval)));
	optval = 10;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, sizeof(optval)));
	optval = 3;
	assert(!setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, sizeof(optval)));
}

void socket_send_init(int fd) {
	// Called by data flow code; NOT safe to assume that fd is a socket
	int res = shutdown(fd, SHUT_RD);
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));

	int optval = 128; // Lowest value that the kernel will accept
	res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));

	optval = 128; // Lowest value that the kernel will accept
	res = setsockopt(fd, IPPROTO_TCP, TCP_WINDOW_CLAMP, &optval, sizeof(optval));
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));

	optval = 60000; // 60s
	res = setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &optval, sizeof(optval));
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));
}

void socket_receive_init(int fd) {
	// Called by data flow code; NOT safe to assume that fd is a socket
	int res = shutdown(fd, SHUT_WR);
	assert(res == 0 || (res == -1 && errno == ENOTSOCK));
}
