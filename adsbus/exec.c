#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "flow.h"
#include "log.h"
#include "opts.h"
#include "peer.h"
#include "receive.h"
#include "send.h"
#include "send_receive.h"
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
static opts_group exec_opts;

static char log_module = 'E';

static void exec_spawn_wrapper(struct peer *);

static void exec_harvest(struct exec *exec) {
	if (exec->child > 0) {
		int status;
		assert(waitpid(exec->child, &status, 0) == exec->child);
		exec->child = -1;
		if (WIFEXITED(status)) {
			LOG(exec->id, "Client exited with status %d", WEXITSTATUS(status));
		} else {
			assert(WIFSIGNALED(status));
			LOG(exec->id, "Client exited with signal %d", WTERMSIG(status));
		}
	}
	peer_close(&exec->log_peer);
}

static void exec_del(struct exec *exec) {
	flow_ref_dec(exec->flow);

	if (exec->child > 0) {
		LOG(exec->id, "Sending SIGTERM to child process %d", exec->child);
		// Racy with the process terminating, so don't assert on it
		kill(exec->child, SIGTERM);
	}
	exec_harvest(exec);
	list_del(&exec->exec_list);
	free(exec->command);
	free(exec);
}

static void exec_close_handler(struct peer *peer) {
	struct exec *exec = container_of(peer, struct exec, peer);
	exec_harvest(exec);
	uint32_t delay = wakeup_get_retry_delay_ms(1);
	LOG(exec->id, "Will retry in %ds", delay / 1000);
	exec->peer.event_handler = exec_spawn_wrapper;
	wakeup_add(&exec->peer, delay);
}

static void exec_log_handler(struct peer *peer) {
	// Do you believe in magic?
	struct exec *exec = container_of(peer, struct exec, log_peer);

	char linebuf[4096];
	ssize_t ret = read(exec->log_peer.fd, linebuf, 4096);
	if (ret <= 0) {
		LOG(exec->id, "Log input stream closed");
		peer_close(&exec->log_peer);
		return;
	}
	size_t len = (size_t) ret;
	char *iter = linebuf, *eol;
	while ((eol = memchr(iter, '\n', len))) {
		assert(eol >= iter);
		size_t linelen = (size_t) (eol - iter);
		LOG(exec->id, "(child output) %.*s", (int) linelen, iter);
		iter += (linelen + 1);
		len -= (linelen + 1);
	}
	if (len) {
		LOG(exec->id, "(child output) %.*s", (int) len, iter);
	}
}

static void exec_parent(struct exec *exec, pid_t child, int data_fd, int log_fd) {
	exec->child = child;
	LOG(exec->id, "Child started as process %d", exec->child);

	exec->log_peer.fd = log_fd;
	exec->log_peer.event_handler = exec_log_handler;
	peer_epoll_add(&exec->log_peer, EPOLLIN);

	exec->peer.event_handler = exec_close_handler;
	if (!flow_new_send_hello(data_fd, exec->flow, exec->passthrough, &exec->peer)) {
		exec_close_handler(&exec->peer);
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
	LOG(exec->id, "Executing: %s", exec->command);
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
	struct exec *exec = container_of(peer, struct exec, peer);
	exec_spawn(exec);
}

static bool exec_add(const char *cmd, struct flow *flow, void *passthrough) {
	exec_new(cmd, flow, passthrough);
	return true;
}

static bool exec_receive(const char *arg) {
	return exec_add(arg, receive_flow, NULL);
}

static bool exec_send(const char *arg) {
	return send_add(exec_add, send_flow, arg);
}

static bool exec_send_receive(const char *arg) {
	return send_add(exec_add, send_receive_flow, arg);
}

void exec_opts_add() {
	opts_add("exec-receive", "COMMAND", exec_receive, exec_opts);
	opts_add("exec-send", "FORMAT=COMMAND", exec_send, exec_opts);
	opts_add("exec-send-receive", "FORMAT=COMMAND", exec_send_receive, exec_opts);
}

void exec_init() {
	opts_call(exec_opts);
}

void exec_cleanup() {
	struct exec *iter, *next;
	list_for_each_entry_safe(iter, next, &exec_head, exec_list) {
		exec_del(iter);
	}
}

void exec_new(const char *command, struct flow *flow, void *passthrough) {
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
