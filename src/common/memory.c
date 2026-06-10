#include <muse/memory.h>
#include <muse/context.h>
#include <muse/elf.h>
#include <muse/logging.h>
#include <muse/paging.h>
#include <muse/sync.h>
#include <muse/trampoline.h>
#include <muse/utils.h>

#include <stdbool.h>

extern void *hpet_base;
extern void *hpet_limit;

extern struct context *active_ctx;

extern struct trampoline_info *t_info;

void memcpy(void *dst, void *src, mem_t size) {
	for (uint32_t i = 0; i < size; i++) {
		((char *)dst)[i] = ((char *)src)[i];
	}
}

int memcmp(const void *s1, const void *s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		unsigned char c1 = ((unsigned char *)s1)[i];
		unsigned char c2 = ((unsigned char *)s2)[i];
		if (c1 != c2) {
			return c1 - c2;
		}
	}
	return 0;
}

struct lock_simple bitmap_lock;

void *kpage_alloc() {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < t_info->region_cnt; i++) {
		paddr_t addr = t_info->regions[i].addr;
		uint32_t idx = 0;
		while (addr < t_info->regions[i].addr +
		                  t_info->regions[i].pg_cnt * PAGE_SIZE) {
			if ((t_info->regions[i].bitmap[idx / 8] >> (idx % 8)) ==
			    0) {
				t_info->regions[i].bitmap[idx / 8] |=
				    1 << (idx % 8);
				return (void *)addr;
			}
			idx++;
			addr += PAGE_SIZE;
		}
	}
	lock_simple_release(&bitmap_lock);
	log(LOG_ERROR, LOG_MEM, "Out of memory!");
	__asm__ volatile("cli; hlt");
	return 0;
}

void kpage_set_status(paddr_t addr, bool free) {
	lock_simple_acquire(&bitmap_lock);
	for (uint32_t i = 0; i < t_info->region_cnt; i++) {
		paddr_t low  = t_info->regions[i].addr;
		paddr_t high = low + t_info->regions[i].pg_cnt * PAGE_SIZE;
		if (addr >= low && addr <= high) {
			uint32_t idx    = (addr - low) / PAGE_SIZE;
			uint8_t *bitmap = &t_info->regions[i].bitmap[idx / 8];
			if (free) {
				*bitmap &= ~(1 << (idx % 8));
			} else {
				*bitmap |= 1 << (idx % 8);
			}
			lock_simple_release(&bitmap_lock);
			return;
		}
	}
}

struct scroll *rsvd_scrolls = 0;

void reserve_scroll(struct scroll *scr) {
	scr->next    = rsvd_scrolls;
	rsvd_scrolls = scr;
}

#define TRAMPOLINE_ALIGNED_HEAP_SIZE (8 * PAGE_SIZE * 8)

void init_memory(struct multiboot_tag_elf_sections *tag_elf) {
	bitmap_lock.stat = 0;

	// Create bitmaps at the start of each free region
	for (uint32_t i = 0; i < t_info->region_cnt; i++) {
		uint32_t bitmap_cnt       = t_info->regions[i].pg_cnt / 8;
		t_info->regions[i].bitmap = (uint8_t *)t_info->limit;
		t_info->limit += bitmap_cnt;
		for (uint32_t j = 0; j < bitmap_cnt; j++) {
			t_info->regions[i].bitmap[j] = 0;
		}
	}

	for (vaddr_t p = (vaddr_t)t_info; p < (vaddr_t)t_info->limit;
	     p += PAGE_SIZE) {
		kpage_set_status(p, false);
	}

	struct scroll kernel_scr = reserve_multiboot_kernel(tag_elf);
	reserve_scroll(&kernel_scr);

	struct scroll t_info_scr;
	t_info_scr.type                 = SCROLL_ALIGNED;
	t_info_scr.vaddr                = (vaddr_t)t_info;
	t_info_scr.aligned_backend.page = (paddr_t)t_info;
	t_info_scr.size =
	    ALIGN_PG_UP((vaddr_t)(t_info->limit - (void *)t_info));

	reserve_scroll(&t_info_scr);

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

	struct heap heap_temp;
	heap_temp.aligned_start = (void *)ALIGN_PG_UP(t_info->limit);
	heap_temp.bitmap_cnt    = TRAMPOLINE_ALIGNED_HEAP_SIZE / PAGE_SIZE / 8;
	heap_temp.max_limit     = (void *)kernel_scr.vaddr;
	heap_temp.limit =
	    heap_temp.aligned_start + TRAMPOLINE_ALIGNED_HEAP_SIZE;
	init_heap(&heap_temp);

	log(LOG_INFO, LOG_MEM, "Trampoline memory initialization complete.\n");
}
