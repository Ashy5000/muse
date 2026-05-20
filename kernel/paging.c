#include "paging.h"
#include "memory.h"
#include "context.h"
#include "scroll.h"
#include "alloc.h"
#include "../drivers/text.h"

#define LOOPBACK_DIR ((paging_table_t)0xFFFFF000)
#define LOOPBACK_TBL(I) ((paging_table_t)(uintptr_t)(0xFFC00000 + 0x400 * (I)))

#define PAGING_BIT_PRESENT 1
#define PAGING_BIT_WRITEABLE 2
#define PAGING_BIT_USER 4

#define MANUSCRIPT_BIND (1 << 6)

#define PG_DIR_IDX(X) ((X) >> 22)
#define PG_TBL_IDX(X) (((X) >> 12) & TEN_BITS)

#define CR0_FLAGS 0x80000001

extern uint32_t reserved_pages_count;
extern uint32_t reserved_pages[MAX_RESERVED_PAGES];

paging_entry_t set_present(paging_entry_t entry, bool present) {
	if (present) {
		return entry | PAGING_BIT_PRESENT;
	}
	return entry & (~PAGING_BIT_PRESENT);
}

paging_entry_t set_writeable(paging_entry_t entry, bool writeable) {
	if (writeable) {
		return entry | PAGING_BIT_WRITEABLE;
	}
	return entry & (~PAGING_BIT_WRITEABLE);
}

paging_entry_t set_addr(paging_entry_t entry, paddr_t addr) {
	return (entry & (~ADDR_MASK)) | (addr & ADDR_MASK);
}

paging_entry_t set_user(paging_entry_t entry, bool user) {
	if (user) {
		return entry | PAGING_BIT_USER;
	}
	return entry & (~PAGING_BIT_USER);
}

paging_entry_t create_paging_entry(vaddr_t addr, bool present, bool writeable, bool user) {
	return set_user(set_writeable(set_present(set_addr(0, addr), present), writeable), user);
}

bool is_present(uint32_t structure) {
	return structure & PAGING_BIT_PRESENT;
}

bool check_table_structure(uint32_t *directory, vaddr_t addr) {
	return is_present(directory[PG_DIR_IDX(addr)]);
}

uint32_t check_or_insert_table_structure(uint32_t *directory, vaddr_t addr, bool user) {
	uint32_t index = PG_DIR_IDX(addr);
	if (is_present(directory[index])) {
		return index;
	}
	uint32_t *table = kpage_alloc();
	directory[index] = create_paging_entry((vaddr_t)table, true, true, user);
	return index;
}

bool check_page_structure(uint32_t *table, vaddr_t addr) {
	return is_present(table[PG_TBL_IDX(addr)]);
}

bool modify_or_insert_page_structure(uint32_t *table, vaddr_t vaddr, paddr_t paddr, bool user) {
	uint32_t index = PG_TBL_IDX(vaddr);
	if (is_present(table[index])) {
		table[index] = set_addr(table[index], paddr);
		return true;
	}
	table[index] = create_paging_entry(paddr, true, true, user);
	return false;
}

void map_page_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr) {
	uint32_t index = check_or_insert_table_structure(directory, vaddr, true);
	modify_or_insert_page_structure((uint32_t*)(uintptr_t)(directory[index] & ADDR_MASK), vaddr, paddr, false);
}

void map_page(vaddr_t vaddr, paddr_t paddr) {
	__asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory" );
	uint32_t index = check_or_insert_table_structure(LOOPBACK_DIR, vaddr, true);
	modify_or_insert_page_structure(LOOPBACK_TBL(index), vaddr, paddr, true);
}

void unmap_page(vaddr_t vaddr) {
	uint32_t directory_idx = PG_DIR_IDX(vaddr);
	if (!is_present(LOOPBACK_DIR[directory_idx])) {
		return;
	}
	uint32_t *table = LOOPBACK_TBL(directory_idx);
	uint32_t table_idx = PG_TBL_IDX(vaddr);
	table[table_idx] = set_present(table[table_idx], false);
	__asm__ volatile ("invlpg (%0)" :: "r"(vaddr) : "memory" );
}

void map_page_range_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr, uint32_t pages) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page_inactive(directory, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE));
	}
}

paddr_t get_page_mapping(vaddr_t vaddr) {
	uint32_t *table = LOOPBACK_TBL(PG_DIR_IDX(vaddr));
	if (!is_present(table[PG_TBL_IDX(vaddr)])) {
		return 0;
	} else {
		return table[PG_TBL_IDX(vaddr)] & ADDR_MASK;
	}
}

void enable_paging(uint32_t* directory) {
	__asm__ volatile ("mov %0, %%cr3" :: "r"(directory) : "memory" );
	__asm__ volatile ("mov %%cr0, %%eax; or %0, %%eax; mov %%eax, %%cr0" :: "r" (CR0_FLAGS) : "eax");
}

paddr_t init_paging() {
	kprint("Building paging structures...\n");
	uint32_t* directory = kpage_alloc();
	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		directory[i] = 0;
	}
	map_page_range_inactive(directory, 0, 0, 1024 * 1024 / PAGE_SIZE);
	for (uint32_t i = 0; i < *entry_count; i++) {
		uint32_t bitmap_count = ((uint32_t*)(uintptr_t)(mmap_table[i].addr_low))[0];
		uint32_t pages = (bitmap_count * sizeof(uint32_t) + PAGE_SIZE - 1) / PAGE_SIZE;
		map_page_range_inactive(directory, mmap_table[i].addr_low, mmap_table[i].addr_low, pages);
	}
	for (uint32_t i = 0; i < reserved_pages_count; i++) {
		map_page_inactive(directory, reserved_pages[i], reserved_pages[i]);
	}
	directory[1023] = create_paging_entry((vaddr_t)directory, true, true, true);
	kprint("Enabling paging...\n");
	enable_paging(directory);
	kprint("Paging enabled.\n");
	return (uintptr_t)directory;
}

void bind_manuscript(manuscript_t manuscript) {
	for (unsigned int i = 0; i < PAGE_SIZE / sizeof(leaf_t); i++) {
		if (manuscript[i] & MANUSCRIPT_BIND) {
			paging_table_t page_table = (paging_table_t)(vaddr_t)(manuscript[i] & ADDR_MASK);
			paddr_t table_paddr = get_page_mapping((vaddr_t)page_table);
			manuscript[i] &= ~ADDR_MASK;
			manuscript[i] |= table_paddr;
			kfree(page_table);
			unmap_page((vaddr_t)page_table);
			manuscript[i] &= ~MANUSCRIPT_BIND;
		}
	}
	unmap_page((vaddr_t)manuscript);
	kfree(manuscript);
}

paddr_t create_task_directory(func_ptr_t func_ptr, bool user, struct scroll *first_scr) {
	struct scroll directory_scr = kmalloc_page();
	paging_table_t directory_virt = (uint32_t*)(uintptr_t)directory_scr.vaddr;

	// Copy the current directory
	for (uint32_t i = 1; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		directory_virt[i] = LOOPBACK_DIR[i];
	}

	// Copy the first table- we need to modify it to add the new stack
	paging_table_t table_active = LOOPBACK_TBL(0);
	struct scroll table_scr = kmalloc_page();
	paging_table_t table_virt = (uint32_t*)(uintptr_t)table_scr.vaddr;

	directory_virt[0] = create_paging_entry(table_scr.aligned_backend.page, true, true, true);

	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(paging_entry_t); i++) {
		table_virt[i] = table_active[i];
	}

	// Map the kernel stack
	for (vaddr_t i = TASK_STACK_BASE - TASK_STACK_SIZE; i <= TASK_STACK_BASE; i += PAGE_SIZE) {
		paddr_t page_phys = (uintptr_t)kpage_alloc();
		table_virt[PG_TBL_IDX(i)] = create_paging_entry(page_phys, true, true, false);
	}

	struct scroll stack_scr = kmalloc_page();
	table_virt[PG_TBL_IDX(TASK_STACK_BASE - PAGE_SIZE)] = create_paging_entry(stack_scr.aligned_backend.page, true, true, false);
	// Fill the kernel stack
	uint32_t *stack = (uint32_t*)(uintptr_t)(stack_scr.vaddr + PAGE_SIZE);
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0; // EBX
	stack[-3] = 0; // ESI
	stack[-4] = 0; // EDI
	stack[-5] = TASK_STACK_BASE; // EBP
	paddr_t page_phys = (uintptr_t)kpage_alloc();
	table_virt[PG_TBL_IDX(TASK_STACK_BASE)] = create_paging_entry(page_phys, true, true, false);

	if (user) {
		// Map the user stack
		for (vaddr_t i = USER_STACK_BASE - USER_STACK_SIZE; i <= USER_STACK_BASE; i += PAGE_SIZE) {
			paddr_t page_phys = (uintptr_t)kpage_alloc();
			table_virt[PG_TBL_IDX(i)] = create_paging_entry(page_phys, true, true, true);
		}
	}

	struct scroll *current_scr = first_scr;
	while (current_scr) {
		uint32_t directory_index = PG_DIR_IDX(current_scr->vaddr);
		paging_table_t alloc_table_virt = table_virt;
		struct scroll alloc_table_scr;
		if (directory_index > 0) {
			if (directory_virt[directory_index]) {
				continue;
			} else {
				alloc_table_scr = kmalloc_page();
				alloc_table_virt = (uint32_t*)(uintptr_t)alloc_table_scr.vaddr;
				directory_virt[directory_index] = create_paging_entry(alloc_table_scr.aligned_backend.page, true, true, true);
			}
		}
		uint32_t table_index = PG_TBL_IDX(current_scr->vaddr);
		alloc_table_virt[table_index] = create_paging_entry(current_scr->aligned_backend.page, true, true, true);
		if (directory_index > 0) {
			scroll_unmap(alloc_table_scr);
		}
		current_scr = current_scr->next;
	}

	scroll_unmap(table_scr);
	scroll_unmap(stack_scr);
	scroll_unmap(directory_scr);


	return directory_scr.aligned_backend.page;
}

void process_page_fault(void) {
	kprint("\n\n\nPage fault: kernel will exit.\n");
	__asm__ volatile ("cli; hlt");
}
