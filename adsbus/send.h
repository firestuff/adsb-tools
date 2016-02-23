#pragma once

struct packet;

void send_init();
void send_cleanup();
struct serializer *send_get_serializer(char *);
void send_add(int, struct serializer *);
void send_add_wrapper(int, void *);
void send_write(struct packet *);
void send_print_usage();
