#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <jansson.h>
#include <protobuf-c/protobuf-c.h>

#include "build.h"
#include "log.h"
#include "opts.h"
#include "uuid.h"

#include "server.h"

#pragma GCC diagnostic ignored "-Wdate-time"

uint8_t server_id[UUID_LEN];
char server_version[] = "https://github.com/flamingcowtv/adsb-tools#1";
static opts_group server_opts1, server_opts2;

static char log_module = 'X';

static bool server_detach(const char __attribute__ ((unused)) *arg) {
	LOG(server_id, "Detaching");

	int ret = fork();
	assert(ret >= 0);
	if (ret > 0) {
		// We are the parent
		exit(EXIT_SUCCESS);
	}

	LOG(server_id, "Background process ID: %u", getpid());

	assert(setsid() >= 0);

	return true;
}

static bool server_pid_file(const char *path) {
	pid_t pid = getpid();
	LOG(server_id, "Writing process ID %d to pid-file: %s", pid, path);
	FILE *fh = fopen(path, "w");
	assert(fh);
	assert(fprintf(fh, "%d\n", pid) > 0);
	assert(!fclose(fh));
	return true;
}

void server_opts_add() {
	opts_add("detach", NULL, server_detach, server_opts1);
	opts_add("pid-file", "PATH", server_pid_file, server_opts2);
}

void server_init() {
	uuid_gen(server_id);
	LOG(server_id, "Server start");
	LOG(server_id, "Build data:");
	LOG(server_id, "\tgit_last_change: %s", GIT_LAST_CHANGE);
	LOG(server_id, "\tgit_local_clean: %s", GIT_LOCAL_CLEAN ? "true" : "false");
	LOG(server_id, "\tclang_version: %s", __clang_version__);
	LOG(server_id, "\tglibc_version: %d.%d", __GLIBC__, __GLIBC_MINOR__);
	LOG(server_id, "\tjansson_version: %s", JANSSON_VERSION);
	LOG(server_id, "\tprotobuf-c_version: %s", PROTOBUF_C_VERSION);
	LOG(server_id, "\tdatetime: %s", __DATE__ " " __TIME__);
	LOG(server_id, "\tusername: %s", USERNAME);
	LOG(server_id, "\thostname: %s", HOSTNAME);

	LOG(server_id, "Runtime data:");
	struct utsname utsname;
	assert(!uname(&utsname));
	cap_t caps = cap_get_proc();
	assert(caps);
	char *caps_str = cap_to_text(caps, NULL);
	assert(caps_str);
	assert(!cap_free(caps));
	LOG(server_id, "\tusername: %s", getlogin());
	LOG(server_id, "\thostname: %s", utsname.nodename);
	LOG(server_id, "\tprocess_id: %d", getpid());
	LOG(server_id, "\tcapabilities: %s", caps_str);
	LOG(server_id, "\tsystem: %s", utsname.sysname);
	LOG(server_id, "\trelease: %s", utsname.release);
	LOG(server_id, "\tversion: %s", utsname.version);
	LOG(server_id, "\tmachine: %s", utsname.machine);
	assert(!cap_free(caps_str));

	opts_call(server_opts1);
	opts_call(server_opts2);
}
