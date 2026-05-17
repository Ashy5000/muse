#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>

struct __attribute__ ((packed)) smap_entry {
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t size_low;
	uint32_t size_high;
	uint32_t type;
	uint32_t acpi;
};

typedef uint32_t mem_t;
typedef mem_t paddr_t;
typedef mem_t vaddr_t;


typedef void(*func_ptr_t)(void);

extern struct smap_entry *mmap_table;
extern uint32_t *entry_count;

struct context;

void memcpy(void *dst, void *src, mem_t size);
void *kpage_alloc(void);
void init_memory(struct context *ctx);

#define MAX_RESERVED_PAGES 16

#endif
