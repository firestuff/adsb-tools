#pragma once

#include <stdint.h>

struct peer;

void wakeup_init(void);
void wakeup_cleanup(void);
int wakeup_get_delay(void);
void wakeup_dispatch(void);
void wakeup_add(struct peer *, uint32_t);
uint32_t wakeup_get_retry_delay_ms(uint32_t);
