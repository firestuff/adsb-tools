#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

#include "rand.h"

struct buf rand_buf = BUF_INIT;
static int rand_fd;

void rand_init() {
	rand_fd = open("/dev/urandom", O_RDONLY);
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

	assert(readv(rand_fd, iov, 2) == rand_buf.start + size);
	rand_buf.start = 0;
	rand_buf.length = BUF_LEN_MAX;
}
