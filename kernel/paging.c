#include "paging.h"
#include "../drivers/text.h"
#include "memory.h"
#include "context.h"

extern uint32_t active_context;
extern struct context contexts[32];

uint32_t *create_page_structure(void) {
	uint32_t *page_structure = kpage_alloc();
	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
		page_structure[i] = 2;
	}
	return page_structure;
}

void map_page_inactive(vaddr_t virt_addr, paddr_t phys_addr, uint32_t flags, uint32_t *page_directory, bool virt) {
	paddr_t directory_index = virt_addr >> 22;
	if ((page_directory[directory_index] & 1) != 1) {
		if (virt) {
			uint32_t *page_table = kpage_alloc();
			uint32_t *page_table_virt = vmalloc_page(0, contexts[active_context]);
			map_page((uintptr_t)page_table_virt, (uintptr_t)page_table, 0);
			for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
				page_table_virt[i] = 6;
			}
			page_directory[directory_index] |= (uintptr_t)page_table;
		} else {
			page_directory[directory_index] |= (uintptr_t)create_page_structure();
		}
		page_directory[directory_index] |= 1;
	}
	uint32_t *page_table = (uint32_t*)(uintptr_t)(page_directory[directory_index] & (~0xffff));
	if (virt) {
		uint32_t *page_table_virt = vmalloc_page(0, contexts[active_context]);
		map_page((uintptr_t)page_table_virt, (uintptr_t)page_table, 0);
		page_table = page_table_virt;
	}
	paddr_t table_index = (virt_addr >> 12) & 0x3ff;
	kprint("Address: p0x");
	kprint_int((uintptr_t)page_table + table_index, 16);
	kprint(".\n");
	page_table[table_index] |= phys_addr | 7 | flags;
}

void map_page_range_inactive(vaddr_t virt_begin, paddr_t phys_begin, uint32_t pages, uint32_t flags, uint32_t *page_directory, bool virt) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page_inactive(virt_begin + (i * PAGE_SIZE), phys_begin + (i * PAGE_SIZE), flags, page_directory, virt);
	}
}

void map_page(vaddr_t virt_addr, paddr_t phys_addr, uint32_t flags) {
	kprint("Mapping v0x");
	kprint_int(virt_addr, 16);
	kprint(" to p0x");
	kprint_int(phys_addr, 16);
	kprint(".\n");
	uint32_t *page_directory = (uint32_t*)0xFFFFF000;
	uint32_t directory_index = virt_addr >> 22;
	kprint("Directory index: ");
	kprint_int(directory_index, 10);
	kprint(".\n");
	if ((page_directory[directory_index] & 1) != 1) {
		kprint("Page table not present. Allocating...\n");
		uint32_t *page_table = kpage_alloc();
		kprint("Allocated page table at p0x");
		kprint_int((uintptr_t)page_table, 16);
		kprint(".\n");
		page_directory[directory_index] |= (uint32_t)(uintptr_t)page_table | 1;
		__asm__ volatile ("push %edx; mov %cr2, %edx; mov %edx, %cr2; pop %edx");
	}
	volatile uint32_t *page_table = (uint32_t*)(uintptr_t)((0x3ff << 22) | (directory_index << 12));
	kprint("Page table address: v0x");
	kprint_int((uintptr_t)page_table, 16);
	kprint(".\n");
	uint32_t table_index = (virt_addr >> 12) & 0x3ff;
	kprint("Page table index: ");
	kprint_int(table_index, 10);
	kprint(".\n");
	page_table[table_index] |= phys_addr | 7 | flags;
	kprint("Page info: 0x");
	kprint_int(page_table[table_index], 16);
	kprint(".\n");
	__asm__ volatile ("push %edx; mov %cr2, %edx; mov %edx, %cr2; pop %edx");
}

void map_page_range(vaddr_t virt_begin, paddr_t phys_begin, uint32_t pages, uint32_t flags) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page(virt_begin + (i * PAGE_SIZE), phys_begin + (i * PAGE_SIZE), flags);
	}
}

bool check_page_status(vaddr_t virt_addr) {
	uint32_t *page_directory = (uint32_t*)0xFFFFF000;
	paddr_t directory_index = virt_addr >> 22;
	if ((page_directory[directory_index] & 1) != 1) {
		kprint("Page table not present. Page not mapped.\n");
		return false;
	}
	uint32_t *page_table = (uint32_t*)(uintptr_t)((0x3ff << 22) | (directory_index << 12));
	kprint("Page table address: v0x");
	kprint_int((uintptr_t)page_table, 16);
	kprint(".\n");
	paddr_t table_index = (virt_addr >> 12) & 0x3ff;
	kprint("Page table index: ");
	kprint_int(table_index, 10);
	kprint(".\n");
	kprint("Page info: 0x");
	kprint_int(page_table[table_index], 16);
	kprint(".\n");
	return (page_table[table_index] & 1) == 1;
}

void process_page_fault(void) {
	kprint("Page fault detected.\n");
	__asm__ volatile ("cli; hlt");
}

uint32_t *init_paging() {
	kprint("Initializing paging...\n");
	uint32_t *page_directory = kpage_alloc();
	for (int i = 0; i < 1024; i++) {
		page_directory[i] = 0x00000006;
	}
	page_directory[PAGE_SIZE / 4 - 1] = (uintptr_t)page_directory | 7;
	map_page_range_inactive(0, 0, 1024 * 1024 / PAGE_SIZE, 0, page_directory, false);
	for (uint32_t i = 0; i < *entry_count; i++) {
		uint32_t num_bitmaps = *((uint32_t*)(uintptr_t)mmap_table[i].addr_low);
		map_page_range_inactive(mmap_table[i].addr_low, mmap_table[i].addr_low, (num_bitmaps + (PAGE_SIZE - 1)) / PAGE_SIZE, 0, page_directory, false);
	}

	kprint("Identity paged 1 MiB.\n");
	kprint_int((uintptr_t)page_directory, 16);
	__asm__ volatile ("hlt");
	__asm__ volatile ("mov %0, %%cr3" :: "r"(page_directory) : "memory" );
	__asm__ volatile ("push %eax; mov %cr0, %eax; or $0x80000001, %eax; mov %eax, %cr0; pop %eax");
	__asm__ volatile ("xchgw %bx, %bx");
	kprint("Paging successfully enabled.\n");
	return (void*)(uintptr_t)0xFFFFF000;
}
