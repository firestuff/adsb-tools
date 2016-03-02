#pragma once

#include <stdbool.h>

struct flow;
struct packet;
struct peer;

void send_init(void);
void send_cleanup(void);
void *send_get_serializer(char *);
void send_write(struct packet *);
void send_print_usage(void);
extern struct flow *send_flow;
