#ifndef ALLOC_H
#define ALLOC_H

#include "memory.h"
#include "paging.h"

struct block_header {
	mem_t size;
	mem_t free; // 0 = in use, 1 = free, 2 = end of memory
};

void *kmalloc(vaddr_t size, struct context ctx);
struct scroll kmalloc_page(struct context ctx);
void kfree(void *p);

#endif
