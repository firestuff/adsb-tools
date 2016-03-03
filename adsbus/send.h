#pragma once

struct buf;
struct flow;
struct packet;

void send_init(void);
void send_cleanup(void);
void *send_get_serializer(char *);
void send_get_hello(struct buf **, void *);
void send_write(struct packet *);
void send_print_usage(void);
extern struct flow *send_flow;
