#include "memory.h"
#include "../drivers/text.h"
#include "paging.h"
#include "context.h"
#include "alloc.h"

#include <stdbool.h>

void kmemcpy(void *dst, void *src, mem_t size) {
	for (int i = 0; i < size; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
}

struct smap_entry *mmap_table = (struct smap_entry*)0x0504;
uint32_t *entry_count = (uint32_t*)0x0500;

uint32_t erase_unusable_regions(uint32_t entry_count) {
	uint32_t entry_count_new = entry_count;
	uint32_t i = 0;
	while (i < entry_count_new) {
		if (mmap_table[i].type == 1) {
			i++;
		} else {
			kmemcpy(mmap_table + i, mmap_table + i + 1, sizeof(struct smap_entry) * (entry_count_new - i - 1));
			entry_count_new--;
		}
	}
	return entry_count_new;
}

void *kpage_alloc() {
	for (uint32_t i = 0; i < *entry_count; i++) {
		uint32_t num_bitmaps = *((uint32_t*)(uintptr_t)mmap_table[i].addr_low);
		mem_t addr = (uintptr_t)mmap_table[i].addr_low + (num_bitmaps + 1) * sizeof(uint32_t);
		addr += PAGE_SIZE - (addr % PAGE_SIZE);
		for (uint32_t j = 0; j < num_bitmaps; j++) {
			uint32_t *bitmap = ((uint32_t*)(uintptr_t)mmap_table[i].addr_low + 1 + j);
			for (uint32_t k = 0; k < 32; k++) {
				if (((*bitmap >> k) & 1) == 0) {
					*bitmap |= 1 << k;
					return (void*)(uintptr_t)addr;
				}
				addr += PAGE_SIZE;
			}
		}
	}
	kprint("Out of memory!");
	__asm__ volatile ("hlt");
	return 0;
}

void init_memory(struct context *ctx) {

	kprint("Memory initialization starting...\n");

	// STAGE I
	// OBJECTIVE: Find the first free region. Remove all non-free regions.

	kprint("MEMORY STAGE I BEGIN\n");

	*entry_count = erase_unusable_regions(*entry_count);

	kprint("Found ");
	kprint_int(*entry_count, 10);
	kprint(" free areas.\n");

	// STAGE II
	// OBJECTIVE: Create bitmaps at the start of each free region

	kprint("MEMORY STAGE II BEGIN\n");

	kprint("Writing bitmaps: ");

	uint32_t bitmaps_written = 0;

	for (uint32_t i = 0; i < *entry_count; i++) {
		mem_t addr = mmap_table[i].addr_low
			+ sizeof(uint32_t);
		mem_t size = mmap_table[i].size_low
			- sizeof(uint32_t);
		uint32_t addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
		uint32_t size_aligned = size - (addr - addr_aligned);
		uint32_t max_pages = size_aligned / PAGE_SIZE;

		uint32_t bitmaps_in_entry = 0;
		for (uint32_t j = 0; j < max_pages; j += sizeof(uint32_t) * 8) {
			*((uint32_t*)(uintptr_t)addr + j + 1) = 0; // 0 = free, 1 = used
			bitmaps_written++;
			bitmaps_in_entry++;
			kput_char('#');
			addr += sizeof(uint32_t);
			size -= sizeof(uint32_t);
			addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
			size_aligned = size - (addr - addr_aligned);
			max_pages = size_aligned / PAGE_SIZE;
		}

		*((uint32_t*)(uintptr_t)mmap_table[0].addr_low) = bitmaps_in_entry;
	}

	kprint("\nWrote ");
	kprint_int(bitmaps_written, 10);
	kprint(" bitmaps to 0x");
	kprint_int(mmap_table[0].addr_low, 16);
	kprint(".\n");

	// STAGE III
	// OBJECTIVE: Intialize paging

	kprint("MEMORY STAGE III BEGIN\n");

	init_paging();

	// STAGE IV
	// OBJECTIVE: Set up kernel heap

	kprint("MEMORY STAGE IV BEGIN\n");
	kprint("Writing header...\n");

	ctx->heap = (void*)0x3001;
	struct block_header *header = ctx->heap;
	header->free = 1;
	header->size = 0x10000;

	kprint("Memory initialization complete.\n");
}
