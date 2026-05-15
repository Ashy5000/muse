#ifndef PAGING_H
#define PAGING_H

#include "scroll.h"

#include <stdint.h>

#define PAGE_SIZE 4096
#define ADDR_MASK 0xFFFFF000
#define TEN_BITS 0x3FF


paddr_t init_paging();
void map_page_inactive(uint32_t* directory, vaddr_t vaddr, paddr_t paddr);
void map_page_range_inactive(uint32_t* directory, vaddr_t vaddr, paddr_t paddr, uint32_t pages);
void map_page(vaddr_t vaddr, paddr_t paddr);
void unmap_page(vaddr_t vaddr);
uint32_t get_page_mapping(vaddr_t vaddr);
paddr_t create_task_directory(func_ptr_t func_ptr, bool user, struct scroll *first_scr);

#endif
