#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>


//////// buf

#define BUF_LEN_MAX 256
struct buf {
	char buf[BUF_LEN_MAX];
	size_t start;
	size_t length;
};
#define BUF_INIT { \
	.start = 0, \
	.length = 0, \
}

#define buf_chr(buff, at) ((buff)->buf[(buff)->start + (at)])
#define buf_at(buff, at) (&buf_chr(buff, at))

ssize_t buf_fill(struct buf *, int);
void buf_consume(struct buf *, size_t);


//////// packet

#define MLAT_MHZ 120
#define RSSI_MAX UINT32_MAX
#define DATA_LEN_MAX 14
struct packet {
	enum {
		MODE_AC,
		MODE_S_SHORT,
		MODE_S_LONG,
	} type;
	char data[DATA_LEN_MAX];
	uint64_t mlat_timestamp;
	uint32_t rssi;
};


//////// backend

#define PARSER_STATE_LEN 256
struct backend;
typedef bool (*parser)(struct backend *, struct packet *);
struct backend {
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser parser;
};
#define BACKEND_INIT { \
	.buf = BUF_INIT, \
	.parser_state = { 0 }, \
	.parser = backend_autodetect_parse, \
}


//////// hex
void hex_init();
void hex_to_bin(char *, char *, size_t);
uint64_t hex_to_int(char *, size_t);
