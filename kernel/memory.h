#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdint.h>

struct __attribute__((packed)) smap_entry {
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t size_low;
	uint32_t size_high;
	uint32_t type;
	uint32_t acpi;
};

typedef uintptr_t mem_t;
typedef mem_t paddr_t;
typedef mem_t vaddr_t;

typedef void (*func_ptr_t)(void);

extern struct smap_entry *mmap_table;
extern uint32_t *entry_count;

struct context;

void memcpy(void *dst, void *src, mem_t size);
void *kpage_alloc(void);
void kpage_set_status(paddr_t addr, bool free);
void init_memory();

#define MAX_RESERVED_PAGES 16

#endif
