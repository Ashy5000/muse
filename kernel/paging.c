#include "paging.h"
#include "memory.h"
#include "context.h"
#include "scroll.h"
#include "alloc.h"
#include "../drivers/text.h"

extern void *hpet_base;
extern void *hpet_limit;
extern uint32_t reserved_pages_count;
extern uint32_t reserved_pages[MAX_RESERVED_PAGES];

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

uint32_t set_user(uint32_t structure, bool user) {
	if (user) {
		return structure | 4;
	}
	return structure & (~4);
}

bool is_present(uint32_t structure) {
	return structure & 1;
}

bool check_table_structure(uint32_t *directory, vaddr_t addr) {
	uint32_t index = addr >> 22;
	return is_present(directory[index]);
}

uint32_t check_or_insert_table_structure(uint32_t *directory, vaddr_t vaddr, bool user) {
	uint32_t index = vaddr >> 22;
	if (is_present(directory[index])) {
		return index;
	}
	uint32_t *table = kpage_alloc();
	directory[index] = set_present(set_writeable(set_page(0, (uintptr_t)table), true), true);
	directory[index] = set_user(directory[index], user);
	return index;
}

bool check_page_structure(uint32_t *table, vaddr_t addr) {
	uint32_t index = (addr >> 12) & TEN_BITS;
	return is_present(table[index]);
}

bool modify_or_insert_page_structure(uint32_t *table, vaddr_t vaddr, paddr_t paddr, bool user) {
	uint32_t index = (vaddr >> 12) & TEN_BITS;
	if (is_present(table[index])) {
		table[index] = set_page(table[index], paddr);
		return true;
	}
	table[index] = set_user(set_present(set_writeable(set_page(0, paddr), true), true), user);
	return false;
}

void map_page_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = check_or_insert_table_structure(directory, vaddr, true);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(directory[index] & ADDR_MASK), vaddr, paddr, true);
}

void map_page(vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = check_or_insert_table_structure((uint32_t*)0xFFFFF000, vaddr, true);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(0xFFC00000 + (index * 0x400)), vaddr, paddr, true);
}

void unmap_page(vaddr_t vaddr) {
	uint32_t *directory = (uint32_t*)0xFFFFF000;
	uint32_t directory_idx = vaddr >> 22;
	if (!is_present(directory[directory_idx])) {
		// Table doesn't exist, nothing to unmap
		return;
	}
	uint32_t *table = (uint32_t*)(uintptr_t)(0xFFC00000 + (directory_idx * 0x400));
	uint32_t table_idx = (vaddr >> 12) & TEN_BITS;
	table[table_idx] = set_present(table[table_idx], false);
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
	kprint("Building paging structures...\n");
	uint32_t* directory = kpage_alloc();
	map_page_range_inactive(directory, 0, 0, 1024 * 1024 / PAGE_SIZE);
	for (int i = 0; i < *entry_count; i++) {
		uint32_t bitmap_count = ((uint32_t*)(uintptr_t)(mmap_table[i].addr_low))[0];
		uint32_t pages = (bitmap_count * sizeof(uint32_t) + PAGE_SIZE - 1) / PAGE_SIZE;
		map_page_range_inactive(directory, mmap_table[i].addr_low, mmap_table[i].addr_low, pages);
	}
	for (uint32_t i = 0; i < reserved_pages_count; i++) {
		map_page_inactive(directory, reserved_pages[i], reserved_pages[i]);
	}
	directory[1023] = set_user(set_present(set_writeable(set_page(0, (uintptr_t)directory), true), true), true);
	kprint("Enabling paging...\n");
	enable_paging(directory);
	kprint("Paging enabled.\n");
	return (uintptr_t)directory;
}

paddr_t create_task_directory(func_ptr_t func_ptr, bool user) {
	paddr_t test = (uintptr_t)kpage_alloc();
	struct scroll directory_scr = kmalloc_page();
	uint32_t *directory_virt = (uint32_t*)(uintptr_t)directory_scr.vaddr;

	// Copy the current directory
	uint32_t *directory_active = (uint32_t*)0xFFFFF000;
	for (uint32_t i = 1; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		directory_virt[i] = directory_active[i];
	}

	// Copy the first table- we need to modify it to add the new stack
	uint32_t* table_active = (uint32_t*)0xFFC00000;
	struct scroll table_scr = kmalloc_page();
	uint32_t* table_virt = (uint32_t*)(uintptr_t)table_scr.vaddr;

	directory_virt[0] = set_user(set_present(set_writeable(set_page(0, table_scr.aligned_backend.page), true), true), true);

	scroll_unmap(directory_scr);

	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		table_virt[i] = table_active[i];
	}

	// Map the kernel stack
	for (uint32_t i = TASK_STACK_BASE - TASK_STACK_SIZE; i <= TASK_STACK_BASE; i += PAGE_SIZE) {
		paddr_t page_phys = (uintptr_t)kpage_alloc();
		table_virt[(i >> 12) & TEN_BITS] = set_user(set_present(set_writeable(set_page(0, page_phys), true), true), true);
	}
	struct scroll stack_scr = kmalloc_page();
	table_virt[((TASK_STACK_BASE - PAGE_SIZE) >> 12) & TEN_BITS] = set_user(set_present(set_writeable(set_page(0, stack_scr.aligned_backend.page), true), true), true);
	// Fill the kernel stack
	uint32_t *stack = (uint32_t*)(uintptr_t)(stack_scr.vaddr + PAGE_SIZE);
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0; // EBX
	stack[-3] = 0; // ESI
	stack[-4] = 0; // EDI
	stack[-5] = TASK_STACK_BASE; // EBP
	paddr_t page_phys = (uintptr_t)kpage_alloc();
	table_virt[(TASK_STACK_BASE >> 12) & TEN_BITS] = set_user(set_present(set_writeable(set_page(0, page_phys), true), true), true);

	if (user) {
		// Map the user stack
		for (uint32_t i = USER_STACK_BASE - USER_STACK_SIZE; i <= USER_STACK_BASE; i += PAGE_SIZE) {
			paddr_t page_phys = (uintptr_t)kpage_alloc();
			table_virt[(i >> 12) & TEN_BITS] = set_user(set_present(set_writeable(set_page(0, page_phys), true), true), true);
		}
	}

	scroll_unmap(stack_scr);
	scroll_unmap(table_scr);

	return directory_scr.aligned_backend.page;
}

void process_page_fault(void) {
	kprint("\n\n\nPage fault: kernel will exit.\n");
	__asm__ volatile ("cli; hlt");
}
