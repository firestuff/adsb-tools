#pragma once

struct flow;

void outgoing_cleanup(void);
void outgoing_new(char *, char *, struct flow *, void *);
