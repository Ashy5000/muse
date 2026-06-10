#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

void init_scheduler(void);
void lock_scheduler(void);
void unlock_scheduler(void);
void schedule(void);
void preempt(void);
void handle_timer(void);
void sleep_secs(uint32_t seconds);
void terminate(void);

#endif
