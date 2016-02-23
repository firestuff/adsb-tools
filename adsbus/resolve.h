#pragma once

struct peer;
struct addrinfo;

void resolve(struct peer *, const char *, const char *, int, struct addrinfo **, const char **);
