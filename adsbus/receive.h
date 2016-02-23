#pragma once

#include <stdbool.h>

#define PARSER_STATE_LEN 256

void receive_cleanup();
void receive_new(int, void *);
void receive_print_usage();
