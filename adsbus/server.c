#include "uuid.h"

#include "log.h"
#include "server.h"

uint8_t server_id[UUID_LEN];
char server_version[] = "https://github.com/flamingcowtv/adsb-tools#1";

void server_init() {
	uuid_gen(server_id);
	log_write('X', server_id, "Server start");
}
