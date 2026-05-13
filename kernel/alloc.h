#ifndef ALLOC_H
#define ALLOC_H

#include "memory.h"
#include "paging.h"

struct block_header {
	mem_t size;
	uint8_t free; // Bit 0: free if set. Bit 1: last header if set.
};

void *kmalloc(vaddr_t size);
void *kmalloc_aligned();
struct scroll kmalloc_page();
void kfree(void *p);

#endif
