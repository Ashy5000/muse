#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "multiboot.h"
#include "scroll.h"

struct mmap_entry {
	size_t addr;
	size_t size;
	bool available;
};

typedef void (*func_ptr_t)(void);

#define MMAP_CNT 8
extern struct mmap_entry mmap_table[MMAP_CNT];

struct context;

void memcpy(void *dst, void *src, mem_t size);
void *kpage_alloc(void);
void kpage_set_status(paddr_t addr, bool free);
void reserve_scroll(struct scroll *scr);
void init_memory(struct multiboot_elf_section_header_table *table);

#define MAX_RESERVED_PAGES 16

#endif
