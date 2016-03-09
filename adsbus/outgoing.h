#pragma once

struct flow;

void outgoing_opts_add(void);
void outgoing_init(void);
void outgoing_cleanup(void);
void outgoing_new(const char *, const char *, struct flow *, void *);
