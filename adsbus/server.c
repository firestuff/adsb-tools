#include <stdlib.h>
#include <sys/types.h>
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
static opts_group server_opts;

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

void server_opts_add() {
	opts_add("detach", NULL, server_detach, server_opts);
}

void server_init() {
	uuid_gen(server_id);
	LOG(server_id, "Server start:");
	LOG(server_id, "\tgit_last_change: %s", GIT_LAST_CHANGE);
	LOG(server_id, "\tgit_local_clean: %s", GIT_LOCAL_CLEAN ? "true" : "false");
	LOG(server_id, "\tclang_version: %s", __clang_version__);
	LOG(server_id, "\tglibc_version: %d.%d", __GLIBC__, __GLIBC_MINOR__);
	LOG(server_id, "\tjansson_version: %s", JANSSON_VERSION);
	LOG(server_id, "\tprotobuf-c_version: %s", PROTOBUF_C_VERSION);
	LOG(server_id, "\tbuild_datetime: %s", __DATE__ " " __TIME__);
	LOG(server_id, "\tbuild_username: %s", USERNAME);
	LOG(server_id, "\tbuild_hostname: %s", HOSTNAME);

	opts_call(server_opts);
}
