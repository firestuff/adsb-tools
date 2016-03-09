#pragma once

#include <stdbool.h>

typedef bool (*opts_handler)(const char *);
typedef char opts_group[1];

void opts_init(int, char *[]);
void opts_add(const char *, const char *, opts_handler, opts_group);
void opts_call(opts_group);
char *opts_split(const char **, char);
