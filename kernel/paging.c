#include "paging.h"
#include "../drivers/text.h"
// #include "memory.h"
// #include "context.h"

#define ADDR_MASK 0xFFFFF000
#define TEN_BITS 0x3FF

uint32_t set_present(uint32_t structure, bool present) {
	if (present) {
		return structure | 1;
	}
	return structure & (~1);
}

uint32_t set_writeable(uint32_t structure, bool writeable) {
	if (writeable) {
		return structure | 2;
	}
	return structure & (~2);
}

uint32_t set_page(uint32_t structure, uint32_t page) {
	return (structure & (~ADDR_MASK)) | (page & ADDR_MASK);
}

bool is_present(uint32_t structure) {
	return structure & 1;
}

bool check_table_structure(uint32_t *directory, vaddr_t addr) {
	uint32_t index = addr >> 22;
	return is_present(directory[index]);
}

uint32_t check_or_insert_table_structure(uint32_t *directory, vaddr_t vaddr) {
	uint32_t index = vaddr >> 22;
	if (is_present(directory[index])) {
		return index;
	}
	uint32_t *table = kpage_alloc();
	directory[index] = set_present(set_writeable(set_page(0, (uintptr_t)table), true), true);
	return index;
}

bool check_page_structure(uint32_t *table, vaddr_t addr) {
	uint32_t index = (addr >> 12) & TEN_BITS;
	return is_present(table[index]);
}

bool modify_or_insert_page_structure(uint32_t *table, vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = (vaddr >> 12) & TEN_BITS;
	if (is_present(table[index])) {
		table[index] = set_page(table[index], paddr);
		return true;
	}
	table[index] = set_present(set_writeable(set_page(0, paddr), true), true);
	return false;
}

void map_page_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = check_or_insert_table_structure(directory, vaddr);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(directory[index] & ADDR_MASK), vaddr, paddr);
}

void map_page(vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = check_or_insert_table_structure((uint32_t*)(0xFFFFF000), vaddr);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(0xFFC00000 + (index * 0x400)), vaddr, paddr);
}

void map_page_range_inactive(uint32_t* directory, vaddr_t vaddr, paddr_t paddr, uint32_t pages) {
	for (int i = 0; i < pages; i++) {
		map_page_inactive(directory, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE));
	}
}

void enable_paging(uint32_t* directory) {
	__asm__ volatile ("mov %0, %%cr3" :: "r"(directory) : "memory" );
	__asm__ volatile ("mov %%cr0, %%eax; or %0, %%eax; mov %%eax, %%cr0" :: "r" (0x80000001) : "eax");
}

void init_paging() {
	uint32_t* directory = kpage_alloc();
	map_page_range_inactive(directory, 0, 0, 1024 * 1024 / PAGE_SIZE);
	directory[1023] = set_present(set_writeable(set_page(0, (uintptr_t)directory), true), true);
	enable_paging(directory);
}

void process_page_fault(void) {
	kprint("\n\n\nPage fault: kernel will exit.\n");
	__asm__ volatile ("cli; hlt");
}
