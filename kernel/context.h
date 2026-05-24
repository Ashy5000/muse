#ifndef CONTEXT_H
#define CONTEXT_H

#include "scroll.h"
#include "vfs.h"

#define FOPEN_MAX 16

struct context {
	mem_t esp;
	paddr_t page_directory;

	bool present;
	struct context *next;

	void *heap; // TODO: Get rid of this
	bool user;
	vaddr_t limit;

	struct file files[FOPEN_MAX];

	uint8_t priority;
	uint32_t alarm;
	uint32_t slices_remaining;
};

void create_context(func_ptr_t func_ptr, uint8_t priority, bool user,
                    struct scroll *first_scr, vaddr_t limit);
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
