#pragma once

struct flow;

void incoming_cleanup(void);
void incoming_new(char *, char *, struct flow *, void *);
