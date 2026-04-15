#ifndef CONTEXT_H
#define CONTEXT_H

#include "memory.h"
#include "paging.h"

struct context {
	mem_t esp;
	paddr_t page_directory;
	bool present;
	void *heap;
	struct context *next;
};

typedef void(*func_ptr_t)(void);

void create_kernel_context(func_ptr_t func_ptr);
void context_switch(struct context *ctx_new);
void init_first_ctx(void);
void schedule(void);
void handle_timer(void);

#define TASK_STACK_BASE 0x200000
#define TASK_STACK_SIZE PAGE_SIZE * 4
#define CTX_COUNT 32


#endif
