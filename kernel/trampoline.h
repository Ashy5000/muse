#ifndef TRAMPOLINE_H
#define TRAMPOLINE_H

#include "acpi.h"
#include "memory.h"

struct trampoline_region {
	vaddr_t addr;
	uint32_t pg_cnt;
	uint8_t *bitmap;
};

#define MMAP_CNT 16

/* A global information structure storing details about the trampoline.
 * Importantly, this information is preserved while moving to the higher half.
 * It is located at the very start of memory. */
struct trampoline_info {
	struct rsdp *rsdp;
	struct trampoline_region regions[MMAP_CNT];
	uint32_t region_cnt;
	void *limit;
};

#endif
