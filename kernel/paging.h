#ifndef PAGING_H
#define PAGING_H

#include "memory.h"

#include <stdint.h>

#define PAGE_SIZE 4096
#define ADDR_MASK 0xFFFFF000
#define TEN_BITS 0x3FF


typedef mem_t paddr_t;
typedef mem_t vaddr_t;

void init_paging();
void map_page_inactive(uint32_t* directory, vaddr_t vaddr, paddr_t paddr);
void map_page_range_inactive(uint32_t* directory, vaddr_t vaddr, paddr_t paddr, uint32_t pages);
void map_page(vaddr_t vaddr, paddr_t paddr);
// void map_page_range(vaddr_t virt_begin, paddr_t phys_begin, uint32_t pages, uint32_t flags);
bool check_page_status(vaddr_t vaddr);

#endif
