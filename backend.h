#pragma once

#include "common.h"


#define PARSER_STATE_LEN 256
struct backend;
typedef bool (*parser)(struct backend *, struct packet *);
struct backend {
	enum peer_type type;
	int fd;
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser parser;
};

#define BACKEND_INIT { \
	.type = PEER_BACKEND, \
	.fd = -1, \
	.buf = BUF_INIT, \
	.parser_state = { 0 }, \
	.parser = backend_autodetect_parse, \
}


bool backend_connect(char *, char *, struct backend *, int);
bool backend_read(struct backend *);
bool backend_autodetect_parse(struct backend *, struct packet *);
