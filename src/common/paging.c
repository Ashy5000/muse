#include <muse/alloc.h>
#include <muse/context.h>
#include <muse/logging.h>
#include <muse/memory.h>
#include <muse/paging.h>
#include <muse/trampoline.h>

#define LOOPBACK_DIR    ((paging_table_t)0xFFFFF000)
#define LOOPBACK_TBL(I) ((paging_table_t)(uintptr_t)(0xFFC00000 + ((I) << 12)))

#define PAGING_BIT_PRESENT   1
#define PAGING_BIT_WRITEABLE 2
#define PAGING_BIT_USER      4

#define MANUSCRIPT_BIND (1 << 10)

#define PG_DIR_IDX(X) ((X) >> 22)
#define PG_TBL_IDX(X) (((X) >> 12) & TEN_BITS)

#define CR0_FLAGS 0x80000001

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

paging_entry_t create_paging_entry(mem_t addr, bool present, bool writeable,
                                   bool user) {
	return set_user(
	    set_writeable(set_present(set_addr(0, addr), present), writeable),
	    user);
}

bool is_present(uint32_t structure) { return structure & PAGING_BIT_PRESENT; }

bool check_table_structure(uint32_t *directory, vaddr_t addr) {
	return is_present(directory[PG_DIR_IDX(addr)]);
}

uint32_t check_or_insert_table_structure(uint32_t *directory, vaddr_t addr,
                                         bool user) {
	uint32_t index = PG_DIR_IDX(addr);
	if (is_present(directory[index])) {
		return index;
	}
	uint32_t *table = kpage_alloc();
	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		table[i] = 0;
	}
	directory[index] =
	    create_paging_entry((vaddr_t)table, true, true, user);
	return index;
}

bool check_page_structure(uint32_t *table, vaddr_t addr) {
	return is_present(table[PG_TBL_IDX(addr)]);
}

bool modify_or_insert_page_structure(uint32_t *table, vaddr_t vaddr,
                                     paddr_t paddr, bool user) {
	uint32_t index = PG_TBL_IDX(vaddr);
	if (is_present(table[index])) {
		table[index] = set_addr(table[index], paddr);
		return true;
	}
	table[index] = create_paging_entry(paddr, true, true, user);
	return false;
}

void map_page_inactive(uint32_t *directory, vaddr_t vaddr, paddr_t paddr) {
	uint32_t index =
	    check_or_insert_table_structure(directory, vaddr, true);
	modify_or_insert_page_structure(
	    (uint32_t *)(uintptr_t)(directory[index] & ADDR_MASK), vaddr, paddr,
	    false);
}

void map_page(vaddr_t vaddr, paddr_t paddr) {
	__asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
	uint32_t index =
	    check_or_insert_table_structure(LOOPBACK_DIR, vaddr, true);
	modify_or_insert_page_structure(LOOPBACK_TBL(index), vaddr, paddr,
	                                true);
}

void unmap_page(vaddr_t vaddr) {
	uint32_t directory_idx = PG_DIR_IDX(vaddr);
	if (!is_present(LOOPBACK_DIR[directory_idx])) {
		return;
	}
	uint32_t *table    = LOOPBACK_TBL(directory_idx);
	uint32_t table_idx = PG_TBL_IDX(vaddr);
	table[table_idx]   = 0;
	__asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

void map_page_range_inactive(paging_table_t directory, vaddr_t vaddr,
                             paddr_t paddr, uint32_t pages) {
	for (uint32_t i = 0; i < pages; i++) {
		map_page_inactive(directory, vaddr + (i * PAGE_SIZE),
		                  paddr + (i * PAGE_SIZE));
	}
}

paging_entry_t get_page_entry(vaddr_t vaddr) {
	uint32_t *table = LOOPBACK_TBL(PG_DIR_IDX(vaddr));
	return table[PG_TBL_IDX(vaddr)];
}

paddr_t get_page_mapping(vaddr_t vaddr) {
	if (!is_present(LOOPBACK_DIR[PG_DIR_IDX(vaddr)])) {
		return 0;
	}
	paging_entry_t entry = get_page_entry(vaddr);
	if (!is_present(entry)) {
		return 0;
	} else {
		return entry & ADDR_MASK;
	}
}

bool check_user(vaddr_t vaddr) {
	paging_entry_t entry = get_page_entry(vaddr);
	return is_present(entry) && (entry & PAGING_BIT_USER);
}

void enable_paging(uint32_t *directory) {
	__asm__ volatile("mov %0, %%cr3" ::"r"(directory) : "memory");
	__asm__ volatile(
	    "mov %%cr0, %%eax; or %0, %%eax; mov %%eax, %%cr0" ::"r"(CR0_FLAGS)
	    : "eax");
}

paddr_t init_paging(struct scroll *scroll) {
	uint32_t *directory = kpage_alloc();
	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
		directory[i] = 0;
	}
	while (scroll) {
		map_page_range_inactive(directory, scroll->vaddr,
		                        scroll->aligned_backend.page,
		                        scroll->size / PAGE_SIZE);
		scroll = scroll->next;
	}
	directory[1023] =
	    create_paging_entry((vaddr_t)directory, true, true, true);
	log(LOG_DEBUG, LOG_PAGING, "Built paging structures.\n");
	enable_paging(directory);
	log(LOG_INFO, LOG_PAGING, "Enabled paging.\n");
	return (uintptr_t)directory;
}

void bind_manuscript(manuscript_t manuscript) {
	for (unsigned int i = 0; i < PAGE_SIZE / sizeof(leaf_t); i++) {
		if (manuscript[i] & MANUSCRIPT_BIND) {
			paging_table_t page_table =
			    (paging_table_t)(vaddr_t)(manuscript[i] &
			                              ADDR_MASK);
			paddr_t table_paddr =
			    get_page_mapping((vaddr_t)page_table);
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

paddr_t create_task_directory(func_ptr_t func_ptr, bool user,
                              struct scroll *scr) {
	struct scroll directory_scr = kmalloc_page();
	paging_table_t directory_virt =
	    (uint32_t *)(uintptr_t)directory_scr.vaddr;

	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t) / 2; i++) {
		directory_virt[i] = 0;
	}
	/* Copy the higher half */
	for (uint32_t i = PAGE_SIZE / sizeof(uint32_t) / 2;
	     i < PAGE_SIZE / sizeof(uint32_t); i++) {
		directory_virt[i] = LOOPBACK_DIR[i];
	}

	directory_virt[(PAGE_SIZE / sizeof(paging_entry_t)) - 1] =
	    create_paging_entry(directory_scr.aligned_backend.page, true, true,
	                        false);

	struct scroll stack_scr = kmalloc_page();
	/* Fill the new stack. */
	uint32_t *stack =
	    (uint32_t *)(uintptr_t)(stack_scr.vaddr +
	                            (TASK_STACK_BASE % PAGE_SIZE));
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0;               // EBX
	stack[-3] = 0;               // ESI
	stack[-4] = 0;               // EDI
	stack[-5] = TASK_STACK_BASE; // EBP
	unmap_page(stack_scr.vaddr);
	kfree((void *)stack_scr.vaddr);
	stack_scr.vaddr = ALIGN_PG_DOWN(TASK_STACK_BASE);
	stack_scr.next  = scr;
	scr             = &stack_scr;

	struct scroll user_stack_scr;
	if (user) {
		user_stack_scr.type                 = SCROLL_ALIGNED;
		user_stack_scr.size                 = USER_STACK_SIZE;
		user_stack_scr.aligned_backend.page = (vaddr_t)kpage_alloc();
		user_stack_scr.vaddr = ALIGN_PG_DOWN(USER_STACK_BASE);
		user_stack_scr.next  = scr;
		scr                  = &user_stack_scr;
	}

	while (scr) {
		uint32_t directory_index = PG_DIR_IDX(scr->vaddr);
		paging_table_t alloc_table_virt;
		if (directory_virt[directory_index] & MANUSCRIPT_BIND) {
			alloc_table_virt =
			    (paging_table_t)(vaddr_t)(directory_virt
			                                  [directory_index] &
			                              ADDR_MASK);
		} else {
			struct scroll alloc_table_scr = kmalloc_page();
			alloc_table_virt =
			    (uint32_t *)(uintptr_t)alloc_table_scr.vaddr;
			directory_virt[directory_index] =
			    create_paging_entry(alloc_table_scr.vaddr, true,
			                        true, true) |
			    MANUSCRIPT_BIND;
			for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t);
			     i++) {
				alloc_table_virt[i] = 0;
			}
		}
		uint32_t table_index          = PG_TBL_IDX(scr->vaddr);
		alloc_table_virt[table_index] = create_paging_entry(
		    scr->aligned_backend.page, true, true, true);
		scr = scr->next;
	}

	bind_manuscript(directory_virt);
	scroll_unmap(directory_scr);

	return directory_scr.aligned_backend.page;
}

paddr_t create_kernel_directory(func_ptr_t func_ptr, struct scroll *scr) {
	struct scroll directory_scr = kmalloc_page();
	paging_table_t directory_virt =
	    (uint32_t *)(uintptr_t)directory_scr.vaddr;

	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t) / 2; i++) {
		directory_virt[i] = LOOPBACK_DIR[i];
	}
	for (uint32_t i = PAGE_SIZE / sizeof(uint32_t) / 2;
	     i < PAGE_SIZE / sizeof(uint32_t) - 1; i++) {
		directory_virt[i] = 0;
	}

	directory_virt[1023] = create_paging_entry(
	    directory_scr.aligned_backend.page, true, true, false);

	struct scroll stack_scr = kmalloc_page();
	/* Fill the new stack. */
	uint32_t *stack =
	    (uint32_t *)(uintptr_t)(stack_scr.vaddr +
	                            (TASK_STACK_BASE % PAGE_SIZE));
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0;               // EBX
	stack[-3] = 0;               // ESI
	stack[-4] = 0;               // EDI
	stack[-5] = TASK_STACK_BASE; // EBP
	unmap_page(stack_scr.vaddr);
	kfree((void *)stack_scr.vaddr);
	stack_scr.vaddr = ALIGN_PG_DOWN(TASK_STACK_BASE);
	stack_scr.next  = scr;
	scr             = &stack_scr;

	while (scr) {
		uint32_t directory_index = PG_DIR_IDX(scr->vaddr);
		paging_table_t alloc_table_virt;
		if (directory_virt[directory_index] & MANUSCRIPT_BIND) {
			alloc_table_virt =
			    (paging_table_t)(vaddr_t)(directory_virt
			                                  [directory_index] &
			                              ADDR_MASK);
		} else {
			struct scroll alloc_table_scr = kmalloc_page();
			alloc_table_virt =
			    (uint32_t *)(uintptr_t)alloc_table_scr.vaddr;
			directory_virt[directory_index] =
			    create_paging_entry(alloc_table_scr.vaddr, true,
			                        true, true) |
			    MANUSCRIPT_BIND;
			for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t);
			     i++) {
				alloc_table_virt[i] = 0;
			}
		}
		uint32_t table_index          = PG_TBL_IDX(scr->vaddr);
		alloc_table_virt[table_index] = create_paging_entry(
		    scr->aligned_backend.page, true, true, true);
		scr = scr->next;
	}

	bind_manuscript(directory_virt);

	scroll_unmap(directory_scr);

	return directory_scr.aligned_backend.page;
}

void free_lower_half() {
	for (uint32_t i = 0; i < PAGE_SIZE / sizeof(paging_entry_t) / 2; i++) {
		if (!is_present(LOOPBACK_DIR[i])) {
			continue;
		}
		paging_table_t page_table = LOOPBACK_TBL(i);
		for (uint32_t j = 0; j < PAGE_SIZE / sizeof(paging_entry_t) / 2;
		     j++) {
			if (is_present(page_table[j])) {
				kpage_set_status(page_table[j] & ADDR_MASK,
				                 true);
			}
			page_table[j] = 0;
		}
		kpage_set_status((vaddr_t)page_table, true);
		LOOPBACK_DIR[i] = 0;
	}
	__asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "%eax");
}

void process_page_fault(void) {
	// TODO: Handle page faults correctly. If it's really irrecoverable, use
	// a dedicated panic() function.
	log(LOG_ERROR, LOG_PAGING, "\n\n\nPage fault: kernel will exit.\n");
	__asm__ volatile("cli; hlt");
}

void *map_phys_obj(void *obj, size_t size) {
	void *limit        = obj + size;
	vaddr_t first_page = ALIGN_PG_DOWN(obj);
	vaddr_t limit_page = ALIGN_PG_UP(limit);
	void *region =
	    kmalloc_aligned_multi((limit_page - first_page) / PAGE_SIZE);
	for (vaddr_t v = first_page; v < limit_page; v += PAGE_SIZE) {
		map_page((vaddr_t)region + v - first_page, v);
	}
	return region + ((vaddr_t)obj % PAGE_SIZE);
}
