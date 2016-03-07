#pragma once

struct flow;

void outgoing_cleanup(void);
void outgoing_new(const char *, const char *, struct flow *, void *);
