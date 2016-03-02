#pragma once

struct addrinfo;

void asyncaddrinfo_init(size_t threads);
void asyncaddrinfo_cleanup(void);
int asyncaddrinfo_resolve(const char *node, const char *service, const struct addrinfo *hints);
int asyncaddrinfo_result(int fd, struct addrinfo **addrs);
