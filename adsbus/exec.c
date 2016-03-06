#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "buf.h"
#include "flow.h"
#include "list.h"
#include "log.h"
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
	flow_ref_dec(exec->flow);

	if (exec->child > 0) {
		log_write('E', exec->id, "Sending SIGTERM to child process %d", exec->child);
		// Racy with the process terminating, so don't assert on it
		kill(exec->child, SIGTERM);
		assert(waitpid(exec->child, NULL, 0) == exec->child);
	}
	list_del(&exec->exec_list);
	free(exec->command);
	free(exec);
}

static void exec_close_handler(struct peer *peer) {
	struct exec *exec = (struct exec *) peer;
	int status;
	assert(waitpid(exec->child, &status, WNOHANG) == exec->child);
	exec->child = -1;
	if (WIFEXITED(status)) {
		log_write('E', exec->id, "Client exited with status %d", WEXITSTATUS(status));
	} else {
		assert(WIFSIGNALED(status));
		log_write('E', exec->id, "Client exited with signal %d", WTERMSIG(status));
	}
	uint32_t delay = wakeup_get_retry_delay_ms(1);
	log_write('E', exec->id, "Will retry in %ds", delay / 1000);
	exec->peer.event_handler = exec_spawn_wrapper;
	wakeup_add((struct peer *) exec, delay);
}

static void exec_parent(struct exec *exec, pid_t child, int fd) {
	exec->child = child;
	log_write('E', exec->id, "Child started as process %d", exec->child);

	exec->peer.event_handler = exec_close_handler;
	if (!flow_new_send_hello(fd, exec->flow, exec->passthrough, (struct peer *) exec)) {
		exec_close_handler((struct peer *) exec);
		return;
	}
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
	log_write('E', exec->id, "Executing: %s", exec->id, exec->command);
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
	flow_ref_inc(flow);

	struct exec *exec = malloc(sizeof(*exec));
	assert(exec);
	exec->peer.fd = -1;
	uuid_gen(exec->id);
	exec->command = strdup(command);
	assert(exec->command);
	exec->flow = flow;
	exec->passthrough = passthrough;

	list_add(&exec->exec_list, &exec_head);

	exec_spawn(exec);
}
