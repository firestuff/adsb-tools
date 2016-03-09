#pragma once

#include <stdbool.h>

struct buf;
struct flow;
struct packet;

void send_init(void);
void send_cleanup(void);
void *send_get_serializer(const char *);
void send_get_hello(struct buf **, void *);
void send_write(struct packet *);
void send_print_usage(void);
bool send_add(bool (*)(const char *, struct flow *, void *), struct flow *, const char *);
extern struct flow *send_flow;
