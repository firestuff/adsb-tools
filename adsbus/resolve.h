#pragma once

struct peer;
struct addrinfo;

void resolve_init(void);
void resolve_cleanup(void);
void resolve(struct peer *, const char *, const char *, int, struct addrinfo **, const char **);
