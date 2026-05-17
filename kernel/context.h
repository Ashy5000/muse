#ifndef CONTEXT_H
#define CONTEXT_H

#include "scroll.h"

struct context {
	mem_t esp;
	paddr_t page_directory;
	bool present;
	void *heap;
	struct context *next;
	uint8_t priority;
	uint32_t alarm;
	uint32_t slices_remaining;
	struct scroll *first_scr;
};

void create_context(func_ptr_t func_ptr, uint8_t priority, bool user, struct scroll *first_scr);
void context_switch(struct context *ctx_new);
void init_first_ctx(void);
void lock_scheduler(void);
void unlock_scheduler(void);
void schedule(void);
void preempt(void);
void handle_timer(void);
void sleep_secs(uint32_t seconds);
void terminate(void);

#define TASK_STACK_BASE 0x9F000
#define TASK_STACK_SIZE PAGE_SIZE
#define USER_STACK_BASE 0x1FFFFF
#define USER_STACK_SIZE PAGE_SIZE

#endif
