#pragma once

#include <stdbool.h>

struct flow;

typedef bool (*opts_handler)(const char *);
typedef char opts_group[1];

void opts_init(int, char *[]);
void opts_add(const char *, const char *, opts_handler, opts_group);
void opts_call(opts_group);
char *opts_split(const char **, char);
bool opts_add_send(bool (*)(const char *, struct flow *, void *), struct flow *, const char *);
