#pragma once

#define PARSER_STATE_LEN 256

struct flow;
struct peer;

void receive_cleanup(void);
void receive_new(int, void *, struct peer *);
void receive_print_usage(void);
extern struct flow *receive_flow;
