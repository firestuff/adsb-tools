#include <assert.h>
#include <signal.h>
#include <stdio.h>
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
	struct peer log_peer;
	uint8_t id[UUID_LEN];
	char *command;
	struct flow *flow;
	void *passthrough;
	pid_t child;
	struct list_head exec_list;
};

static struct list_head exec_head = LIST_HEAD_INIT(exec_head);

static void exec_spawn_wrapper(struct peer *);

static void exec_harvest(struct exec *exec) {
	if (exec->child > 0) {
		int status;
		assert(waitpid(exec->child, &status, 0) == exec->child);
		exec->child = -1;
		if (WIFEXITED(status)) {
			log_write('E', exec->id, "Client exited with status %d", WEXITSTATUS(status));
		} else {
			assert(WIFSIGNALED(status));
			log_write('E', exec->id, "Client exited with signal %d", WTERMSIG(status));
		}
	}
	if (exec->log_peer.fd >= 0) {
		assert(!close(exec->log_peer.fd));
		exec->log_peer.fd = -1;
	}
}

static void exec_del(struct exec *exec) {
	flow_ref_dec(exec->flow);

	if (exec->child > 0) {
		log_write('E', exec->id, "Sending SIGTERM to child process %d", exec->child);
		// Racy with the process terminating, so don't assert on it
		kill(exec->child, SIGTERM);
	}
	exec_harvest(exec);
	list_del(&exec->exec_list);
	free(exec->command);
	free(exec);
}

static void exec_close_handler(struct peer *peer) {
	struct exec *exec = (struct exec *) peer;
	exec_harvest(exec);
	uint32_t delay = wakeup_get_retry_delay_ms(1);
	log_write('E', exec->id, "Will retry in %ds", delay / 1000);
	exec->peer.event_handler = exec_spawn_wrapper;
	wakeup_add((struct peer *) exec, delay);
}

static void exec_log_handler(struct peer *peer) {
	// Do you believe in magic?
	struct exec *exec = container_of(peer, struct exec, log_peer);

	char linebuf[4096];
	ssize_t ret = read(exec->log_peer.fd, linebuf, 4096);
	if (ret <= 0) {
		log_write('E', exec->id, "Log input stream closed");
		assert(!close(exec->log_peer.fd));
		exec->log_peer.fd = -1;
		return;
	}
	size_t len = (size_t) ret;
	char *iter = linebuf, *eol;
	while ((eol = memchr(iter, '\n', len))) {
		assert(eol >= iter);
		size_t linelen = (size_t) (eol - iter);
		log_write('E', exec->id, "(child output) %.*s", (int) linelen, iter);
		iter += (linelen + 1);
		len -= (linelen + 1);
	}
	if (len) {
		log_write('E', exec->id, "(child output) %.*s", (int) len, iter);
	}
}

static void exec_parent(struct exec *exec, pid_t child, int data_fd, int log_fd) {
	exec->child = child;
	log_write('E', exec->id, "Child started as process %d", exec->child);

	exec->log_peer.fd = log_fd;
	exec->log_peer.event_handler = exec_log_handler;
	peer_epoll_add(&exec->log_peer, EPOLLIN);

	exec->peer.event_handler = exec_close_handler;
	if (!flow_new_send_hello(data_fd, exec->flow, exec->passthrough, (struct peer *) exec)) {
		exec_close_handler((struct peer *) exec);
		return;
	}
}

static void __attribute__ ((noreturn)) exec_child(const struct exec *exec, int data_fd, int log_fd) {
	assert(setsid() != -1);
	// We leave stderr open from child to parent
	// Other than that, fds should have CLOEXEC set
	if (data_fd != STDIN_FILENO) {
		assert(dup2(data_fd, STDIN_FILENO) == STDIN_FILENO);
	}
	if (data_fd != STDOUT_FILENO) {
		assert(dup2(data_fd, STDOUT_FILENO) == STDOUT_FILENO);
	}
	if (data_fd != STDIN_FILENO && data_fd != STDOUT_FILENO) {
		assert(!close(data_fd));
	}
	if (log_fd != STDERR_FILENO) {
		assert(dup2(log_fd, STDERR_FILENO) == STDERR_FILENO);
		assert(!close(log_fd));
	}
	assert(!execl("/bin/sh", "sh", "-c", exec->command, NULL));
	abort();
}

static void exec_spawn(struct exec *exec) {
	log_write('E', exec->id, "Executing: %s", exec->command);
	int data_fds[2], log_fds[2];
	// Leave these sockets blocking; we move in lock step with subprograms
	assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, data_fds));
	assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, log_fds));
	
	int res = fork();
	assert(res >= 0);
	if (res) {
		assert(!close(data_fds[1]));
		assert(!close(log_fds[1]));
		assert(!shutdown(log_fds[0], SHUT_WR));
		exec_parent(exec, res, data_fds[0], log_fds[0]);
	} else {
		assert(!close(data_fds[0]));
		assert(!close(log_fds[0]));
		exec_child(exec, data_fds[1], log_fds[1]);
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
