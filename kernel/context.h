#ifndef CONTEXT_H
#define CONTEXT_H

#include "memory.h"

struct context {
	uint32_t *page_directory;
	void *heap;
	mem_t esp;
	mem_t ebp;
};

typedef void(*func_ptr_t)(void);

struct context create_user_context(func_ptr_t func_ptr);
void context_switch(struct context);

#endif
