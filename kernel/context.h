#ifndef CONTEXT_H
#define CONTEXT_H

#include "memory.h"
#include "paging.h"

struct context {
	mem_t esp;
	paddr_t page_directory;
	bool present;
	void *heap;
};

typedef void(*func_ptr_t)(void);

uint32_t create_user_context(func_ptr_t func_ptr, struct context ctx);
void context_switch(struct context *ctx_new);
void init_kernel_ctx(void);

#define USERSPACE_STACK_BASE 0x200000
#define USERSPACE_STACK_SIZE PAGE_SIZE * 4
#define CTX_COUNT 32


#endif
