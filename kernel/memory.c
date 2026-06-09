#include "memory.h"
#include "alloc.h"
#include "context.h"
#include "elf.h"
#include "logging.h"
#include "paging.h"
#include "sync.h"

#include <stdbool.h>

extern void *hpet_base;
extern void *hpet_limit;

extern struct context *active_ctx;

void memcpy(void *dst, void *src, mem_t size) {
	for (uint32_t i = 0; i < size; i++) {
		((char *)dst)[i] = ((char *)src)[i];
	}
}

struct mmap_entry mmap_table[MMAP_CNT];

struct lock_simple bitmap_lock;

uint32_t get_bitmap_count(uint32_t idx) {
	return *((uint32_t *)mmap_table[idx].addr);
}

void *kpage_alloc() {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < MMAP_CNT; i++) {
		if (!mmap_table[i].available) {
			break;
		}
		uint32_t num_bitmaps = get_bitmap_count(i);
		paddr_t addr =
		    mmap_table[i].addr + (num_bitmaps + 1) * sizeof(uint32_t);
		addr += PAGE_SIZE - (addr % PAGE_SIZE);
		for (uint32_t j = 0; j < num_bitmaps; j++) {
			uint32_t *bitmap =
			    (uint32_t *)(mmap_table[i].addr) + 1 + j;
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
	log(LOG_ERROR, LOG_MEM, "Out of memory!");
	__asm__ volatile("cli; hlt");
	return 0;
}

void kpage_set_status(paddr_t addr, bool free) {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < MMAP_CNT; i++) {
		if (!mmap_table[i].available) {
			break;
		}
		uint32_t num_bitmaps = *((uint32_t *)mmap_table[i].addr);
		paddr_t low =
		    mmap_table[i].addr + (num_bitmaps + 1) * sizeof(uint32_t);
		paddr_t high = mmap_table[i].addr + mmap_table[i].size;
		if (addr >= low && addr <= high) {
			paddr_t offset      = addr - low;
			uint32_t bitmap_idx = offset / (PAGE_SIZE * 32);
			uint32_t *bitmap =
			    (uint32_t *)(mmap_table[i].addr + 1 + bitmap_idx);
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

struct scroll *rsvd_scrolls = 0;

struct scroll kernel_scr;

void reserve_scroll(struct scroll *scr) {
	scr->next    = rsvd_scrolls;
	rsvd_scrolls = scr;
}

void init_memory(struct multiboot_tag_elf_sections *tag_elf) {
	bitmap_lock.stat = 0;

	// Create bitmaps at the start of each free region
	for (uint32_t i = 0; i < MMAP_CNT; i++) {
		if (!mmap_table[i].available) {
			break;
		}
		mem_t addr            = mmap_table[i].addr + sizeof(uint32_t);
		mem_t size            = mmap_table[i].size - sizeof(uint32_t);
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

		*((uint32_t *)mmap_table[i].addr) = bitmaps_in_entry;
		log(LOG_INFO, LOG_MEM, "Wrote physical alloc bitmaps to %x.\n",
		    mmap_table[i].addr);
	}

	kernel_scr = reserve_multiboot_kernel(tag_elf);
	reserve_scroll(&kernel_scr);

	// Intialize paging
	active_ctx->page_directory = init_paging(rsvd_scrolls);

	/* All of this is completely arbitrary. This might be the worst-written
	 * piece of code in the entire OS. FIXME!!!!*/
	// struct heap heap_temp;
	// heap_temp.limit         = (void *)0x10000;
	// heap_temp.max_limit     = (void *)0x90000;
	// heap_temp.bitmap_cnt    = 10;
	// heap_temp.aligned_start = (void *)(ALIGN_PG_UP(
	//     mmap_table[0].addr +
	//     sizeof(uint32_t) *
	// 	(1 + *((uint32_t *)(vaddr_t)mmap_table[0].addr))));
	// init_heap(&heap_temp);
}
