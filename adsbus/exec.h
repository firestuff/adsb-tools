#pragma once

struct flow;

void exec_opts_add(void);
void exec_init(void);
void exec_cleanup(void);
void exec_new(const char *, struct flow *, void *);
