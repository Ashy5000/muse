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

#ifdef BITS64

typedef uint64_t mem_t;

#else

typedef uint32_t mem_t;

#endif

extern struct smap_entry *mmap_table;
extern uint32_t *entry_count;

struct context;

void kmemcpy(void *dst, void *src, mem_t size);
void *kpage_alloc(void);
void init_memory(struct context *ctx);
void *vmalloc(mem_t size, uint32_t flags, struct context ctx);
void *vmalloc_page(uint32_t flags, struct context ctx);
void *kmalloc(mem_t size, uint32_t flags, struct context ctx);
void kfree(void *ptr);

#endif
