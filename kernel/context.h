#ifndef CONTEXT_H
#define CONTEXT_H

#include "memory.h"
#include "paging.h"

struct context {
	paddr_t page_directory;
	void *heap;
	mem_t esp;
	mem_t ebp;
};

typedef void(*func_ptr_t)(void);

struct context create_user_context(func_ptr_t func_ptr, struct context ctx);
void context_switch(struct context, bool kernel);

#define USERSPACE_STACK_BASE 0x200000
#define USERSPACE_STACK_SIZE PAGE_SIZE * 4


#endif
