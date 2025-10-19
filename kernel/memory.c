#include "memory.h"
#include "../drivers/text.h"
#include "paging.h"

#include <stdint.h>
#include <stdbool.h>

void kmemcpy(void *dst, void *src, unsigned long bytes) {
	for (int i = 0; i < bytes; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
}

struct block_header {
	int size;
	int free; // 0 = in use, 1 = free, 2 = end of memory
};

void *MEM_START;

struct __attribute__ ((packed)) smap_entry {
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t size_low;
	uint32_t size_high;
	uint32_t type;
	uint32_t acpi;
};

struct smap_entry *mmap_table = (struct smap_entry*)0x0504;
uint32_t *entry_count = (uint32_t*)0x0500;

uintptr_t first_illegal_address(int entry_count) {
	uintptr_t lowest_illegal_address = -1;

	for (int i = 0; i < entry_count; i++) {
		uintptr_t addr = mmap_table[i].addr_low;
		uintptr_t size = mmap_table[i].size_low;

		if (mmap_table[i].type == 1) {
			if (i == 0 || addr + size < lowest_illegal_address) {
				lowest_illegal_address = addr + size;
			}
		} else if (i == 0 || addr < lowest_illegal_address) {
			lowest_illegal_address = addr;
		}
	}
	
	return lowest_illegal_address;
}

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
		uint32_t addr = (uint32_t)(uintptr_t)mmap_table[i].addr_low + (num_bitmaps + 1) * sizeof(uint32_t);
		addr -= PAGE_SIZE - (addr % PAGE_SIZE);
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
	return 0;
}

void init_memory(void) {

	kprint("Memory initialization starting...\n");

	// STAGE I
	// OBJECTIVE: Find the first free region. Remove all non-free regions.
	
	kprint("MEMORY STAGE I BEGIN\n");

	uintptr_t first_free_start = (uintptr_t)(mmap_table + *entry_count); // Start at the end of the table
	uintptr_t first_free_end = first_illegal_address(*entry_count); // Extend as far as we can
	
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
		uint32_t addr = mmap_table[i].addr_low + sizeof(uint32_t);
		uint32_t size = mmap_table[i].size_low - sizeof(uint32_t);
		uint32_t addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
		uint32_t size_aligned = size - (addr - addr_aligned);
		uint32_t max_pages = size_aligned / PAGE_SIZE;

		for (uint32_t j = 0; j < max_pages; j += sizeof(uint32_t) * 8) {
			*((uint32_t*)(uintptr_t)addr + j + 1) = 0; // 0 = free, 1 = used
			bitmaps_written++;
			kput_char('#');
			addr += sizeof(uint32_t);
			size -= sizeof(uint32_t);
			addr_aligned = addr - PAGE_SIZE + (addr % PAGE_SIZE);
			size_aligned = size - (addr - addr_aligned);
			max_pages = size_aligned / PAGE_SIZE;
		}

		*((uint32_t*)(uintptr_t)mmap_table[i].addr_low) = max_pages;
	}

	kprint("\nWrote ");
	kprint_int(bitmaps_written, 10);
	kprint(" bitmaps.\n");

	// STAGE III
	// OBJECTIVE: Create page structure
	
	kprint("MEMORY STAGE III BEGIN\n");

	init_paging();

	kprint("Memory initialization complete.\n");
}

void fuse_blocks(struct block_header *header) {
	struct block_header *header_next = (void*)header + sizeof(struct block_header) + header->size;
	if (header_next->free == 1 || header_next->free == 3) {
		header->size += sizeof(struct block_header) + header_next->size;
	}
	header->free = 1;
}

void *kmalloc(long size) {
	void *block = MEM_START;
	struct block_header *header;
	while(1) {
		header = block;
		if (header->free == 2) {
			return 0;
		}
		if (header->free == 1) {
			fuse_blocks(header);
		}
		if (header->size >= size && header->free) {
			if (header->size - size > sizeof(struct block_header)) {
				struct block_header *new_header = block + size + sizeof(struct block_header);
				new_header->size = header->size - size - sizeof(struct block_header);
				new_header->free = 1;
			}
			header->size = size;
			header->free = 0;
			return (void*)header + sizeof(struct block_header);
		} else {
			block += header->size + sizeof(struct block_header);
		}
	}
}

void kfree(void *ptr) {
	struct block_header *header = ptr - sizeof(struct block_header);
	header->free = 1;
}
