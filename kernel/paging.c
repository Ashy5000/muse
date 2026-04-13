#include "paging.h"
#include "memory.h"
#include "context.h"
#include "scroll.h"
#include "alloc.h"
#include "../drivers/text.h"
#include "../drivers/hpet.h"

extern void *hpet_base;
extern void *hpet_limit;

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
	uint32_t index = check_or_insert_table_structure((uint32_t*)0xFFFFF000, vaddr);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(0xFFC00000 + (index * 0x400)), vaddr, paddr);
}

void map_page_range_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr, uint32_t pages) {
	for (int i = 0; i < pages; i++) {
		map_page_inactive(directory, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE));
	}
}

paddr_t get_page_mapping(vaddr_t vaddr) {
	uint32_t directory_index = (vaddr >> 22) & TEN_BITS;
	uint32_t table_index = (vaddr >> 12) & TEN_BITS;
	uint32_t *table = (uint32_t*)(uintptr_t)(0xFFC00000 + (directory_index * 0x400));
	if (!is_present(table[table_index])) {
		return 0;
	} else {
		return table[table_index] & ADDR_MASK;
	}
}

void enable_paging(uint32_t* directory) {
	__asm__ volatile ("mov %0, %%cr3" :: "r"(directory) : "memory" );
	__asm__ volatile ("mov %%cr0, %%eax; or %0, %%eax; mov %%eax, %%cr0" :: "r" (0x80000001) : "eax");
}

paddr_t init_paging() {
	uint32_t* directory = kpage_alloc();
	map_page_range_inactive(directory, 0, 0, 1024 * 1024 / PAGE_SIZE);
	for (int i = 0; i < *entry_count; i++) {
		uint32_t bitmap_count = ((uint32_t*)(uintptr_t)(mmap_table[i].addr_low))[0];
		uint32_t pages = (bitmap_count * sizeof(uint32_t) + PAGE_SIZE - 1) / PAGE_SIZE;
		map_page_range_inactive(directory, mmap_table[i].addr_low, mmap_table[i].addr_low, pages);
	}
	uint32_t hpet_page_start = (uintptr_t)hpet_base - ((uintptr_t)hpet_base % PAGE_SIZE);
	uint32_t hpet_page_end = (uintptr_t)hpet_limit + PAGE_SIZE - ((uintptr_t)hpet_limit % PAGE_SIZE);
	map_page_range_inactive(directory, hpet_page_start, hpet_page_start, (hpet_page_end - hpet_page_start) / PAGE_SIZE);
	directory[1023] = set_present(set_writeable(set_page(0, (uintptr_t)directory), true), true);
	enable_paging(directory);
	return (uintptr_t)directory;
}

paddr_t create_user_directory(struct context ctx) {
	struct scroll directory_scr = kmalloc_page(ctx);
	uint32_t* directory_virt = (uint32_t*)(uintptr_t)directory_scr.vaddr;
	// The first table should encompass everything we need
	uint32_t* table_kernel = (uint32_t*)0xFFC00000;
	struct scroll table_scr = kmalloc_page(ctx);
	uint32_t* table_virt = (uint32_t*)(uintptr_t)table_scr.vaddr;
	directory_virt[0] = set_present(set_writeable(set_page(0, table_scr.aligned_backend.page), true), true);
	for (int i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		table_virt[i] = table_kernel[i];
	}
	// Map the new stack
	for (int i = TASK_STACK_BASE - TASK_STACK_SIZE + PAGE_SIZE; i <= TASK_STACK_BASE; i += PAGE_SIZE) {
		paddr_t page_phys = (uintptr_t)kpage_alloc();
		table_virt[(i >> 12) & TEN_BITS] = set_present(set_writeable(set_page(0, page_phys), true), true);
	}
	return directory_scr.aligned_backend.page;
}

void process_page_fault(void) {
	kprint("\n\n\nPage fault: kernel will exit.\n");
	__asm__ volatile ("cli; hlt");
}
