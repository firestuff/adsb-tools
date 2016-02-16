#pragma once

#include <stdint.h>
#include <unistd.h>


//////// peer

// All specific peer structs must be castable to this.
struct peer {
	enum peer_type {
		PEER_BACKEND,
		PEER_CLIENT,
	} type;
};


//////// buf

#define BUF_LEN_MAX 256
struct buf {
	char buf[BUF_LEN_MAX];
	size_t start;
	size_t length;
};

#define buf_chr(buff, at) ((buff)->buf[(buff)->start + (at)])
#define buf_at(buff, at) (&buf_chr(buff, at))

void buf_init(struct buf *);
ssize_t buf_fill(struct buf *, int);
void buf_consume(struct buf *, size_t);


//////// packet

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


//////// mlat

#define MLAT_MHZ 120
#define MLAT_MAX UINT64_MAX
#define RSSI_MAX UINT32_MAX

struct mlat_state {
	uint64_t timestamp_last;
	uint64_t timestamp_generation;
};

uint64_t mlat_timestamp_scale_in(uint64_t, uint64_t, uint16_t, struct mlat_state *);


//////// rssi

uint32_t rssi_scale_in(uint32_t, uint32_t);


//////// hex

void hex_init();
void hex_to_bin(char *, char *, size_t);
uint64_t hex_to_int(char *, size_t);
void hex_from_bin(char *, char *, size_t);


///////// uuid

#define UUID_LEN 37
void uuid_gen(char *);
