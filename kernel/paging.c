#include "paging.h"
#include "../drivers/text.h"
#include "memory.h"

#include <stdint.h>

struct __attribute__ ((packed)) page_directory_entry {
	unsigned int present : 1;
	unsigned int writeable : 1;
	unsigned int user_accessible : 1;
	unsigned int write_through : 1;
	unsigned int disable_cache : 1;
	unsigned int accessed : 1;
	unsigned int unused_0 : 1;
	unsigned int page_size : 1;
	unsigned int unused_1 : 4;
	unsigned int addr : 20;
};

struct page_directory_entry *page_directory;

struct __attribute__ ((packed)) page_table_entry {
	unsigned int present : 1;
	unsigned int writeable : 1;
	unsigned int user_accessible : 1;
	unsigned int write_through : 1;
	unsigned int disable_cache : 1;
	unsigned int accessed : 1;
	unsigned int dirty : 1;
	unsigned int pat : 1;
	unsigned int global : 1;
	unsigned int unused : 3;
	unsigned int addr : 20;
};

struct page_table_entry *create_page_table() {
	struct page_table_entry *page_table = kpage_alloc();
	kprint("Page table allocated at 0x");
	kprint_int((uintptr_t)page_table, 16);
	kprint(".\n");
	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
		struct page_table_entry *entry = page_table + i;
		entry->present = 0;
		entry->writeable = 1;
		entry->user_accessible = 1;
		entry->write_through = 0;
		entry->disable_cache = 0;
		entry->accessed = 0;
		entry->dirty = 0;
		entry->pat = 0;
		entry->global = 0;
		entry->unused = 0;
		entry->addr = 0xfffff;
	}
	return page_table;
}

void map_page(uint32_t virt_addr, uint32_t phys_addr) {
	uint32_t directory_index = virt_addr >> 22;
	if (page_directory[directory_index].addr == 0xfffff) {
		page_directory[directory_index].addr = (uintptr_t)create_page_table() >> 12;
	}
	struct page_table_entry *page_table = (struct page_table_entry*)(uintptr_t)(page_directory[directory_index].addr << 12);
	uint32_t table_index = (virt_addr >> 12) & 0b1111111111;
	page_table[table_index].addr = phys_addr;
}

void map_page_range(uint32_t virt_begin, uint32_t phys_begin, uint32_t pages) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page(virt_begin + i, phys_begin + i);
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
		struct page_directory_entry *entry = page_directory + i;
		entry->present = 0;
		entry->writeable = 1;
		entry->user_accessible = 1;
		entry->write_through = 0;
		entry->disable_cache = 0;
		entry->accessed = 0;
		entry->unused_0 = 0;
		entry->page_size = 0;
		entry->unused_1 = 0;
		entry->addr = 0xfffff;
	}
	map_page_range(0, 0, 1024 * 1024 / PAGE_SIZE);
	kprint("Identity paged 1 MiB.\n");
	__asm__ ("mov %%cr3, %0" : "=a"(page_directory) : : );
	__asm__ ("push %eax; mov %eax, %cr0; or %eax, 0x80000001; mov %cr0, %eax");
	kprint("Paging successfully enabled.\n");
}
