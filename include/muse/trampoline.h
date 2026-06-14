#ifndef TRAMPOLINE_H
#define TRAMPOLINE_H

#include <muse/memory.h>
#include <muse/vga.h>

struct trampoline_region {
	vaddr_t addr;
	uint32_t pg_cnt;
	uint8_t *bitmap;
};

#define MMAP_CNT 16

/* A global information structure storing details about the prekernel.
 * Importantly, this information is preserved while moving to the higher half.
 * It is located at the very start of memory. */
struct trampoline_info {
	struct vga_framebuffer fb;
	struct trampoline_region regions[MMAP_CNT];
	uint32_t region_cnt;
	void *kernel_limit;
	void *limit;
};

#endif
