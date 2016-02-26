#pragma once

struct packet;
struct peer;

void send_init(void);
void send_cleanup(void);
struct serializer *send_get_serializer(char *);
void send_new(int, struct serializer *, struct peer *);
void send_new_wrapper(int, void *, struct peer *);
void send_write(struct packet *);
void send_print_usage(void);
