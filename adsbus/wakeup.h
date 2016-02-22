#pragma once

struct peer;

void wakeup_init();
void wakeup_cleanup();
int wakeup_get_delay();
void wakeup_dispatch();
void wakeup_add(struct peer *, uint32_t);
