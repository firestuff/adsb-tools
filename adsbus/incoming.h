#pragma once

struct flow;

void incoming_opts_add(void);
void incoming_init(void);
void incoming_cleanup(void);
void incoming_new(const char *, const char *, struct flow *, void *);
