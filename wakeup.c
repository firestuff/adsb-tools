#include <assert.h>
#include <unistd.h>

#include "common.h"

#include "wakeup.h"

void wakeup_init() {
	int pipefd[2];
	assert(!pipe(pipefd));
}

void wakeup_add(struct peer *peer, int delay_ms) {
}
