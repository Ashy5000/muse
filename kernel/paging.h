#ifndef PAGING_H
#define PAGING_H

#include "memory.h"

#include <stdint.h>

#define PAGE_SIZE 4096

typedef mem_t paddr_t;
typedef mem_t vaddr_t;

uint32_t *init_paging();
uint32_t *create_page_structure();
void map_page_inactive(vaddr_t virt_addr, paddr_t phys_addr, uint32_t flags, uint32_t *page_directory, bool virt);
void map_page_range_inactive(vaddr_t virt_addr, paddr_t phys_addr, uint32_t pages, uint32_t flags, uint32_t *page_directory, bool virt);
void map_page(vaddr_t virt_addr, paddr_t phys_addr, uint32_t flags);
void map_page_range(vaddr_t virt_begin, paddr_t phys_begin, uint32_t pages, uint32_t flags);
bool check_page_status(vaddr_t virt_addr);

#endif
