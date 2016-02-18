#pragma once

#include <stdbool.h>

#include "common.h"

#define PARSER_STATE_LEN 256

void backend_new(const char *, const char *);
void backend_new_fd(int);
void backend_new_fd_wrapper(int, void *);
void backend_print_usage();
