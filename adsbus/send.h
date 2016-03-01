#pragma once

#include <stdbool.h>

struct buf;
struct flow;
struct packet;
struct peer;

void send_init(void);
void send_cleanup(void);
struct serializer *send_get_serializer(char *);
void send_new(int, struct serializer *, struct peer *);
void send_new_wrapper(int, void *, struct peer *);
bool send_new_hello(int, struct serializer *, struct peer *);
void send_hello(struct buf **, void *);
void send_write(struct packet *);
void send_print_usage(void);
extern struct flow *send_flow;
