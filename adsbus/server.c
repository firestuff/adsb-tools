#include <stdio.h>

#include "uuid.h"

#include "server.h"

char server_id[UUID_LEN];
char server_version[] = "https://github.com/flamingcowtv/adsb-tools#1";

void server_init() {
	uuid_gen(server_id);
	fprintf(stderr, "X %s: Server start\n", server_id);
}
