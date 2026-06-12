#include <muse/alloc.h>
#include <muse/context.h>
#include <muse/logging.h>
#include <muse/memory.h>
#include <muse/paging.h>

extern struct context *active_ctx;

/* This function does what sbrk() does for malloc() in userspace but in the
 * kernel. It expands the end of the heap. */
void *heap_sbrk(intptr_t inc) {
	void *prev_lim = active_ctx->ctx_heap->limit;
	if (prev_lim + inc > active_ctx->ctx_heap->max_limit) {
		log(LOG_ERROR, LOG_ALLOC, "Out of memory!\n");
		__asm__ volatile("cli; hlt");
	}
	for (vaddr_t v = ALIGN_PG_DOWN(active_ctx->ctx_heap->limit);
	     v <= ALIGN_PG_DOWN(active_ctx->ctx_heap->limit + inc);
	     v += PAGE_SIZE) {
		if (!get_page_mapping(v)) {
			map_page(v, (paddr_t)kpage_alloc());
		}
	}
	active_ctx->ctx_heap->limit += inc;
	return prev_lim;
}

void init_heap(struct heap *temp_heap) {
	temp_heap->size = 0;
	temp_heap->free = 0;
	for (unsigned int i = 0; i < TIER_CNT; i++) {
		temp_heap->tiers[i] = 0;
	}
	active_ctx->ctx_heap   = temp_heap;
	// There is always a prev_size stored at the end of the heap
	size_t *root_prev_size = heap_sbrk(sizeof(*root_prev_size));
	temp_heap->limit = (void *)root_prev_size + sizeof(*root_prev_size);
	*root_prev_size  = 0;
	temp_heap->page_bitmap =
	    kmalloc(sizeof(uint32_t) * temp_heap->bitmap_cnt);
	for (unsigned int i = 0; i < temp_heap->bitmap_cnt; i++) {
		temp_heap->page_bitmap[i] = 0;
	}
	struct heap *ctx_heap = kmalloc(sizeof(*ctx_heap));
	*ctx_heap             = *temp_heap;
	active_ctx->ctx_heap  = ctx_heap;
}

void split_chunk(struct chunk *ch, size_t size) {
	/* These chunks are not next to each other in the linked list: they are
	 * adjacent in memory. */
	struct chunk *next_ch =
	    (void *)ch +
	    (ch->size & SIZE_MASK); /* The chunk after the one being split. */
	struct chunk *new_ch =
	    (void *)ch + size +
	    CHUNK_OVERHEAD; /* The new chunk being created in the split. */

	/* Calculate the size of the new chunk. This *includes* its
	 * overhead. */
	new_ch->size =
	    ((ch->size & SIZE_MASK) - size - CHUNK_OVERHEAD) | BIT_FREE;
	ch->size           = size + CHUNK_OVERHEAD;
	new_ch->prev_size  = ch->size;

	next_ch->prev_size = new_ch->size; /* Because we are changing the size
	                                      of chunks, we need to inform the
	                                      next one of our changes. */

	/* Put the new chunk into the correct tier. */
	unsigned int idx   = TIER_IDX(new_ch);
	new_ch->next       = active_ctx->ctx_heap->tiers[idx];
	/* TODO: Don't completely obliterate the cache. */
	active_ctx->ctx_heap->tiers[idx] = new_ch;
}

void *kmalloc(size_t size) {
	size_t ch_size = size + CHUNK_OVERHEAD >= MIN_CHUNK_SIZE
	                     ? size + CHUNK_OVERHEAD
	                     : MIN_CHUNK_SIZE;
	unsigned int i =
	    TIER_IDX(ch_size); /* The index of the tier the chunk will be in. */
	struct chunk *ch =
	    active_ctx->ctx_heap->tiers[i]; /* The chunk we are examining. */
	struct chunk *prev = 0;             /* The previous chunk. */
	while (ch) {
		if ((ch->size & SIZE_MASK) < ch_size) {
			/* There isn't enough room in the chunk. */
			prev = ch;
			ch   = ch->next;
			continue;
		}
		/* Remove from linked list */
		if (prev) {
			prev->next = ch->next;
		} else {
			active_ctx->ctx_heap->tiers[i] = ch->next;
		}

		/* Is it worth it to split into two chunks?
		   TODO: Parametrize this better */
		if ((ch->size & SIZE_MASK) >=
		    ch_size + MIN_CHUNK_SIZE + CHUNK_OVERHEAD) {
			split_chunk(ch, size);
		}
		void *res = (void *)ch + CHUNK_OVERHEAD;
		return res;
	}

	struct chunk *new_ch = active_ctx->ctx_heap->limit -
	                       sizeof(size_t); /* Overlap with prev_size stored
	                                at the end of the heap */
	heap_sbrk(ch_size); /* Make sure to allocate a new terminating
	                                       prev_size */
	new_ch->size = ch_size;
	active_ctx->ctx_heap->size += size;
	void *res = (void *)new_ch + CHUNK_OVERHEAD;
	return res;
}

void kfree(void *ptr) {
	if (ptr >= active_ctx->ctx_heap->aligned_start) {
		size_t offset = ptr - active_ctx->ctx_heap->aligned_start;
		if (offset <
		    active_ctx->ctx_heap->bitmap_cnt * 32 * PAGE_SIZE) {
			active_ctx->ctx_heap
			    ->page_bitmap[offset / (32 * PAGE_SIZE)] |=
			    1 << ((offset / PAGE_SIZE) % 32);
			return;
		}
	}
	struct chunk *ch = ptr - CHUNK_OVERHEAD;
	if (ch->prev_size & BIT_FREE) {
		struct chunk *merged_ch =
		    (void *)ch - (ch->prev_size & SIZE_MASK);
		merged_ch->size += ch->size;
		ch = merged_ch;
	}
	ch->size |= BIT_FREE;
	unsigned int i                 = TIER_IDX(ch->size & SIZE_MASK);
	ch->next                       = active_ctx->ctx_heap->tiers[i];
	active_ctx->ctx_heap->tiers[i] = ch;
	active_ctx->ctx_heap->free += ch->size & SIZE_MASK;
}

void *kmalloc_aligned() {
	for (uint32_t i = 0; i < active_ctx->ctx_heap->bitmap_cnt; i++) {
		for (uint32_t j = 0; j < 32; j++) {
			if (!(active_ctx->ctx_heap->page_bitmap[i] >> j)) {
				active_ctx->ctx_heap->page_bitmap[i] |= 1 << j;
				void *res =
				    active_ctx->ctx_heap->aligned_start +
				    (i * 32 + j) * PAGE_SIZE;
				log(LOG_DEBUG, LOG_ALLOC,
				    "kmalloc_aligned() returning %x.\n", res);
				return res;
			}
		}
	}
	log(LOG_ERROR, LOG_ALLOC, "Out of virtual page-sized chunks!\n");
	__asm__ volatile("cli; hlt");
	return 0;
}

struct scroll kmalloc_page() {
	vaddr_t page_virt = (vaddr_t)kmalloc_aligned();
	struct scroll scr;
	scr.size                 = 0;
	scr.type                 = SCROLL_FAILED;
	scr.aligned_backend.page = 0;
	scr.vaddr                = 0;
	if (!page_virt) {
		return scr;
	}

	paddr_t page_phys = (paddr_t)kpage_alloc();
	if (!page_phys) {
		return scr;
	}

	map_page(page_virt, page_phys);

	scr.size                 = PAGE_SIZE;
	scr.type                 = SCROLL_ALIGNED;
	scr.vaddr                = page_virt;
	scr.aligned_backend.page = page_phys;
	log(LOG_DEBUG, LOG_ALLOC, "Allocated page: v%x->p%x.\n", scr.vaddr,
	    scr.aligned_backend.page);
	return scr;
}
