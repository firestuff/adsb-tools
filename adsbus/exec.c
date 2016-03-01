#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "buf.h"
#include "flow.h"
#include "list.h"
#include "peer.h"
#include "uuid.h"
#include "wakeup.h"

#include "exec.h"

struct exec {
	struct peer peer;
	uint8_t id[UUID_LEN];
	char *command;
	struct flow *flow;
	void *passthrough;
	pid_t child;
	struct list_head exec_list;
};

static struct list_head exec_head = LIST_HEAD_INIT(exec_head);

static void exec_spawn_wrapper(struct peer *);

static void exec_del(struct exec *exec) {
	(*exec->flow->ref_count)--;
	if (exec->child > 0) {
		fprintf(stderr, "E %s: Sending SIGTERM to child process %d\n", exec->id, exec->child);
		// Racy with the process terminating, so don't assert on it
		kill(exec->child, SIGTERM);
		assert(waitpid(exec->child, NULL, 0) == exec->child);
	}
	free(exec->command);
	free(exec);
}

static void exec_close_handler(struct peer *peer) {
	struct exec *exec = (struct exec *) peer;
	int status;
	assert(waitpid(exec->child, &status, WNOHANG) == exec->child);
	exec->child = -1;
	if (WIFEXITED(status)) {
		fprintf(stderr, "E %s: Client exited with status %d\n", exec->id, WEXITSTATUS(status));
	} else {
		assert(WIFSIGNALED(status));
		fprintf(stderr, "E %s: Client exited with signal %d\n", exec->id, WTERMSIG(status));
	}
	uint32_t delay = wakeup_get_retry_delay_ms(1);
	fprintf(stderr, "E %s: Will retry in %ds\n", exec->id, delay / 1000);
	exec->peer.event_handler = exec_spawn_wrapper;
	wakeup_add((struct peer *) exec, delay);
}

static void exec_parent(struct exec *exec, pid_t child, int fd) {
	exec->child = child;
	fprintf(stderr, "E %s: Child started as process %d\n", exec->id, exec->child);

	if (!flow_hello(fd, exec->flow, exec->passthrough)) {
		assert(!close(fd));
		exec_close_handler((struct peer *) exec);
		return;
	}

	exec->peer.event_handler = exec_close_handler;
	exec->flow->new(fd, exec->passthrough, (struct peer *) exec);
}

static void __attribute__ ((noreturn)) exec_child(const struct exec *exec, int fd) {
	assert(setsid() != -1);
	// We leave stderr open from child to parent
	// Other than that, fds should have CLOEXEC set
	if (fd != 0) {
		assert(dup2(fd, 0) == 0);
	}
	if (fd != 1) {
		assert(dup2(fd, 1) == 1);
	}
	if (fd != 0 && fd != 1) {
		assert(!close(fd));
	}
	assert(!execl("/bin/sh", "sh", "-c", exec->command, NULL));
	abort();
}

static void exec_spawn(struct exec *exec) {
	fprintf(stderr, "E %s: Executing: %s\n", exec->id, exec->command);
	int fds[2];
	assert(!socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds));
	
	int res = fork();
	assert(res >= 0);
	if (res) {
		assert(!close(fds[1]));
		exec_parent(exec, res, fds[0]);
	} else {
		assert(!close(fds[0]));
		exec_child(exec, fds[1]);
	}
}

static void exec_spawn_wrapper(struct peer *peer) {
	struct exec *exec = (struct exec *) peer;
	exec_spawn(exec);
}

void exec_cleanup() {
	struct exec *iter, *next;
	list_for_each_entry_safe(iter, next, &exec_head, exec_list) {
		exec_del(iter);
	}
}

void exec_new(char *command, struct flow *flow, void *passthrough) {
	(*flow->ref_count)++;

	struct exec *exec = malloc(sizeof(*exec));
	exec->peer.fd = -1;
	uuid_gen(exec->id);
	exec->command = strdup(command);
	exec->flow = flow;
	exec->passthrough = passthrough;

	list_add(&exec->exec_list, &exec_head);

	exec_spawn(exec);
}
