#ifndef PAGING_H
#define PAGING_H

#include <muse/memory.h>
#include <muse/scroll.h>

#include <stdint.h>

#define PAGE_SIZE 4096
#define ADDR_MASK 0xFFFFF000
#define TEN_BITS  0x3FF

#define ALIGN_PG_DOWN(X) ((vaddr_t)(X) - ((vaddr_t)(X) % PAGE_SIZE))
/* FIXME: Don't modify address if it's already aligned */
#define ALIGN_PG_UP(X)                                                         \
	((vaddr_t)(X) % PAGE_SIZE > 0 ? ALIGN_PG_DOWN((vaddr_t)X) + PAGE_SIZE  \
	                              : (vaddr_t)(X))

typedef uint32_t paging_entry_t;
typedef paging_entry_t *paging_table_t;
typedef paging_entry_t leaf_t;
typedef leaf_t *manuscript_t;

paddr_t init_paging(struct scroll *first_scr);
void map_page_inactive(paging_table_t directory, vaddr_t vaddr, paddr_t paddr);
void map_page_range_inactive(paging_table_t directory, vaddr_t vaddr,
                             paddr_t paddr, uint32_t pages);
void map_page(vaddr_t vaddr, paddr_t paddr);
void unmap_page(vaddr_t vaddr);
paddr_t get_page_mapping(vaddr_t vaddr);
bool check_user(vaddr_t vaddr);
paddr_t create_task_directory(func_ptr_t func_ptr, bool user,
                              struct scroll *first_scr);
paddr_t create_kernel_directory(func_ptr_t func_ptr, struct scroll *first_scr);

#endif
