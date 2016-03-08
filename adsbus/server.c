#include "uuid.h"

#include "log.h"
#include "server.h"

uint8_t server_id[UUID_LEN];
char server_version[] = "https://github.com/flamingcowtv/adsb-tools#1";

static char log_module = 'X';

void server_init() {
	uuid_gen(server_id);
	LOG(server_id, "Server start");
}
