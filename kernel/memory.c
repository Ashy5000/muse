#include "memory.h"
#include "alloc.h"
#include "context.h"
#include "logging.h"
#include "paging.h"
#include "sync.h"

#include <stdbool.h>

extern void *hpet_base;
extern void *hpet_limit;

uint32_t reserved_pages_count = 0;
uint32_t reserved_pages[MAX_RESERVED_PAGES];

void memcpy(void *dst, void *src, mem_t size) {
	for (uint32_t i = 0; i < size; i++) {
		((char *)dst)[i] = ((char *)src)[i];
	}
}

struct smap_entry *mmap_table = (struct smap_entry *)0x0504;
uint32_t *entry_count         = (uint32_t *)0x0500;

uint32_t erase_unusable_regions(uint32_t entry_count) {
	uint32_t entry_count_new = entry_count;
	uint32_t i               = 0;
	while (i < entry_count_new) {
		if (mmap_table[i].type == 1 && mmap_table[i].addr_low > 0) {
			i++;
		} else {
			memcpy(mmap_table + i, mmap_table + i + 1,
			       sizeof(struct smap_entry) *
			           (entry_count_new - i - 1));
			entry_count_new--;
		}
	}
	return entry_count_new;
}

struct lock_simple bitmap_lock;

uint32_t get_bitmap_count(uint32_t idx) {
	return *((uint32_t *)(uintptr_t)mmap_table[idx].addr_low);
}

void *kpage_alloc() {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < *entry_count; i++) {
		uint32_t num_bitmaps = get_bitmap_count(i);
		paddr_t addr         = (uintptr_t)mmap_table[i].addr_low +
		                       (num_bitmaps + 1) * sizeof(uint32_t);
		addr += PAGE_SIZE - (addr % PAGE_SIZE);
		for (uint32_t j = 0; j < num_bitmaps; j++) {
			uint32_t *bitmap =
			    (uint32_t *)(uintptr_t)(mmap_table[i].addr_low) +
			    1 + j;
			for (uint32_t k = 0; k < 32; k++) {
				if (((*bitmap >> k) & 1) == 0) {
					*bitmap |= 1 << k;
					lock_simple_release(&bitmap_lock);
					return (void *)(uintptr_t)addr;
				}
				addr += PAGE_SIZE;
			}
		}
	}
	lock_simple_release(&bitmap_lock);
	log(LOG_WARN, LOG_MEM, "Out of memory!");
	__asm__ volatile("hlt");
	return 0;
}

void kpage_set_status(paddr_t addr, bool free) {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < *entry_count; i++) {
		uint32_t num_bitmaps =
		    *((uint32_t *)(uintptr_t)mmap_table[i].addr_low);
		paddr_t low  = (uintptr_t)mmap_table[i].addr_low +
		               (num_bitmaps + 1) * sizeof(uint32_t);
		paddr_t high = mmap_table[i].addr_low + mmap_table[i].size_low;
		if (addr >= low && addr <= high) {
			paddr_t offset      = addr - low;
			uint32_t bitmap_idx = offset / (PAGE_SIZE * 32);
			uint32_t *bitmap =
			    (uint32_t *)(uintptr_t)(mmap_table[i].addr_low + 1 +
			                            bitmap_idx);
			uint32_t bit_idx =
			    (offset % (PAGE_SIZE * 32)) / PAGE_SIZE;
			if (free) {
				*bitmap &= ~(1 << bit_idx);
			} else {
				*bitmap |= 1 << bit_idx;
			}
			lock_simple_release(&bitmap_lock);
			return;
		}
	}
}

void init_memory(struct context *ctx) {
	// Find the first free region. Remove all non-free regions
	*entry_count = erase_unusable_regions(*entry_count);
	log(LOG_INFO, LOG_MEM, "Found %i free areas.\n", *entry_count);

	bitmap_lock.stat = 0;

	// Create bitmaps at the start of each free region
	for (uint32_t i = 0; i < *entry_count; i++) {
		mem_t addr = mmap_table[i].addr_low + sizeof(uint32_t);
		mem_t size = mmap_table[i].size_low - sizeof(uint32_t);
		uint32_t addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
		uint32_t size_aligned = size - (addr - addr_aligned);
		uint32_t max_pages    = size_aligned / PAGE_SIZE;

		uint32_t bitmaps_in_entry = 0;
		for (uint32_t j = 0; j < max_pages; j += sizeof(uint32_t) * 8) {
			*((uint32_t *)(uintptr_t)addr + j + 1) =
			    0; // 0 = free, 1 = used
			bitmaps_in_entry++;
			addr += sizeof(uint32_t);
			size -= sizeof(uint32_t);
			addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
			size_aligned = size - (addr - addr_aligned);
			max_pages    = size_aligned / PAGE_SIZE;
		}

		*((uint32_t *)(uintptr_t)mmap_table[0].addr_low) =
		    bitmaps_in_entry;
	}

	log(LOG_INFO, LOG_MEM, "Wrote physical alloc bitmaps to %x.\n",
	    mmap_table[0].addr_low);

	for (uint32_t i = 0; i < reserved_pages_count; i++) {
		kpage_set_status(reserved_pages[i], false);
	}

	// Intialize paging
	ctx->page_directory         = init_paging();

	// STAGE IV
	// OBJECTIVE: Set up kernel heap

	ctx->heap                   = (void *)0x10000;
	struct block_header *header = ctx->heap;
	header->free                = 3;
	header->size = 0x30000; // TODO: actually base this on something
#ifdef ALLOC_CANARY
	memcpy(header->canary, "MUSE", 4);
#endif
}
