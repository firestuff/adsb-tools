#pragma once

#include "common.h"


#define PARSER_STATE_LEN 256
struct backend;
typedef bool (*parser)(struct backend *, struct packet *);
struct backend {
	enum peer_type type;
	char id[UUID_LEN];
	int fd;
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser parser;
};


void backend_init(struct backend *);
bool backend_connect(char *, char *, struct backend *, int);
bool backend_read(struct backend *);
