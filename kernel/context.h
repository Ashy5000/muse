#ifndef CONTEXT_H
#define CONTEXT_H

#include "memory.h"
#include "paging.h"

struct context {
	bool present;
	paddr_t page_directory;
	void *heap;
	mem_t esp;
	mem_t ebp;
	mem_t instruction_ptr;
};

typedef void(*func_ptr_t)(void);

uint32_t create_user_context(func_ptr_t func_ptr, struct context ctx);
void context_switch(uint32_t ctx_index_new);
void init_kernel_ctx(void);

#define USERSPACE_STACK_BASE 0x200000
#define USERSPACE_STACK_SIZE PAGE_SIZE * 4
#define CTX_COUNT 32


#endif
