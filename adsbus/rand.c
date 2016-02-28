#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "buf.h"

#include "rand.h"

static struct buf rand_buf = BUF_INIT;
static int rand_fd;

void rand_init() {
	rand_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	assert(rand_fd >= 0);
	assert(read(rand_fd, buf_at(&rand_buf, 0), BUF_LEN_MAX) == BUF_LEN_MAX);
	rand_buf.length = BUF_LEN_MAX;
}

void rand_cleanup() {
	assert(!close(rand_fd));
}

void rand_fill(void *value, size_t size) {
	if (size <= rand_buf.length) {
		memcpy(value, buf_at(&rand_buf, 0), size);
		buf_consume(&rand_buf, size);
		return;
	}

	struct iovec iov[2] = {
		{
			.iov_base = rand_buf.buf,
			.iov_len = rand_buf.start,
		},
		{
			.iov_base = value,
			.iov_len = size,
		},
	};

	size_t bytes = rand_buf.start + size;
	assert(bytes < SSIZE_MAX);
	assert(readv(rand_fd, iov, 2) == (ssize_t) bytes);
	rand_buf.start = 0;
	rand_buf.length = BUF_LEN_MAX;
}
