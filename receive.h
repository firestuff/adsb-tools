#pragma once

#include <stdbool.h>

#include "common.h"

#define PARSER_STATE_LEN 256

void receive_new(int, void *);
void receive_print_usage();
