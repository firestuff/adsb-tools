#pragma once

#define PARSER_STATE_LEN 256

struct flow;

void receive_cleanup(void);
void receive_print_usage(void);
extern struct flow *receive_flow;
