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

struct context;

void memcpy(void *dst, void *src, mem_t size);
int memcmp(const void *s1, const void *s2, size_t n);
void *kpage_alloc(void);
void kpage_set_status(paddr_t addr, bool free);
void reserve_scroll(struct scroll *scr);
void init_memory(struct multiboot_tag_elf_sections *tag_elf);

#endif
