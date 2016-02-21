#pragma once

struct peer;

void wakeup_init();
void wakeup_cleanup();
void wakeup_add(struct peer *, int);
