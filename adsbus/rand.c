#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include "rand.h"

static int rand_fd;

void rand_init() {
	rand_fd = open("/dev/urandom", O_RDONLY);
	assert(rand_fd >= 0);
}

void rand_cleanup() {
	assert(!close(rand_fd));
}

void rand_fill(void *value, size_t size) {
	assert(read(rand_fd, value, size) == size);
}
