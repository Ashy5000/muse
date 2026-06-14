#ifndef ALLOC_H
#define ALLOC_H

#include <muse/memory.h>
#include <muse/paging.h>
#include <stddef.h>

#define ALLOC_CANARY

#define MIN_CHUNK_SIZE_LOG 4
#define MIN_CHUNK_SIZE     (1 << MIN_CHUNK_SIZE_LOG)
#define TIER_IDX(X) ((31 - __builtin_clz((uintptr_t)X)) - MIN_CHUNK_SIZE_LOG)
#define CHUNK_SIZE_LIMIT_LOG 25
#define TIER_CNT             CHUNK_SIZE_LIMIT_LOG - MIN_CHUNK_SIZE_LOG
#define CHUNK_OVERHEAD       (2 * sizeof(size_t))
#define BIT_FREE             1
#define SIZE_MASK            (~BIT_FREE)

struct heap {
	/* For variable-size allocations */
	size_t size;
	size_t free;
	struct chunk *tiers[TIER_CNT];
	void *limit;
	void *max_limit;

	/* For specifically allocating 1-page chunks */
	uint32_t *page_bitmap;
	uint32_t bitmap_cnt;
	void *aligned_start;
};

struct chunk {
	size_t prev_size;
	size_t size; /* Includes overhead */
	struct chunk *next;
	// The end of the chunk contains an identical copy of size.
	// This makes combining chunks faster.
};

void *kmalloc(size_t size);
void *kmalloc_aligned();
struct scroll kmalloc_page();
void *kmalloc_aligned_multi(uint32_t cnt);
void kfree(void *p);
void init_heap(struct heap *temp_heap);

#endif
