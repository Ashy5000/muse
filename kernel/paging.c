#include "paging.h"
#include "../drivers/text.h"
#include "memory.h"

#include <stdint.h>

uint32_t *page_directory;

uint32_t *create_page_table() {
	uint32_t *page_table = kpage_alloc();
	kprint("Page table allocated at 0x");
	kprint_int((uintptr_t)page_table, 16);
	kprint(".\n");
	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
		page_table[i] = 6;
	}
	return page_table;
}

void map_page(uint32_t virt_addr, uint32_t phys_addr) {
	__asm__ volatile ("invlpg (%0)" :: "r"(virt_addr) : "memory");
	uint32_t directory_index = virt_addr >> 22;
	if ((page_directory[directory_index] & 1) != 1) {
		page_directory[directory_index] |= (uint32_t)(uintptr_t)create_page_table();
		page_directory[directory_index] |= 1;
	}
	uint32_t *page_table = (uint32_t*)(uintptr_t)(page_directory[directory_index] >> 12 << 12);
	uint32_t table_index = (virt_addr >> 12) & 0b1111111111;
	page_table[table_index] |= phys_addr;
	page_table[table_index] |= 1;
}

void map_page_range(uint32_t virt_begin, uint32_t phys_begin, uint32_t pages) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page(virt_begin + (i * PAGE_SIZE), phys_begin + (i * PAGE_SIZE));
	}
}

void init_paging(void) {
	kprint("Initializing paging...\n");
	page_directory = kpage_alloc();
	if (page_directory == 0) {
		kprint("Directory allocation failed!\n");
	}
	kprint("Page directory allocated at 0x");
	kprint_int((int)(uintptr_t)(page_directory), 16);
	kprint(".\n");
	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
		page_directory[i] = 6;
	}
	map_page_range(0, 0, 1024 * 1024 / PAGE_SIZE);
	kprint("Identity paged 1 MiB.\n");
	// while (1) {}
	__asm__ volatile ("mov %0, %%cr3" :: "r"(page_directory) : "memory" );
	__asm__ volatile ("push %eax; mov %cr0, %eax; or $0x80000000, %eax; mov %eax, %cr0; pop %eax");
	kprint("Paging successfully enabled.\n");
}
