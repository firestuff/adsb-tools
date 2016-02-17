#pragma once

#include <stdbool.h>

#include "common.h"


#define PARSER_STATE_LEN 256
struct backend;
struct addrinfo;
typedef bool (*parser)(struct backend *, struct packet *);
struct backend {
	struct peer peer;
	char id[UUID_LEN];
	char *node;
	char *service;
	struct addrinfo *addrs;
	struct addrinfo *addr;
	struct buf buf;
	char parser_state[PARSER_STATE_LEN];
	parser parser;
};

struct backend *backend_new(char *node, char *service, int epoll_fd);
