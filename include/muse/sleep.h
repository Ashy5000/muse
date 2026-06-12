#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>

void sleep_tick();
void sleep_millis(uint32_t millis);
void sleep_secs(uint32_t seconds);

#endif
