#define KERNEL

#include <muse/alloc.h>
#include <muse/context.h>
#include <muse/logging.h>
#include <muse/psf.h>
#include <muse/text.h>
#include <muse/trampoline.h>

struct trampoline_info *t_info = 0;
extern bool logging_enabled;
extern struct vga_framebuffer fb;

#define KERNEL_ALIGNED_HEAP_SIZE (512 * PAGE_SIZE)

int kmain() {
	logging_enabled = false;

	init_first_ctx();

	struct heap heap_temp;
	heap_temp.bitmap_cnt    = KERNEL_ALIGNED_HEAP_SIZE / PAGE_SIZE / 8;
	heap_temp.aligned_start = (void *)ALIGN_PG_UP(t_info->kernel_limit);
	heap_temp.max_limit     = (void *)TASK_STACK_BASE - TASK_STACK_SIZE + 1;
	heap_temp.limit = heap_temp.aligned_start + KERNEL_ALIGNED_HEAP_SIZE;
	init_heap(&heap_temp);
	struct trampoline_info info_tmp = *t_info;
	t_info                          = kmalloc(sizeof(*t_info));
	*t_info                         = info_tmp;

	for (uint32_t i = 0; i < t_info->region_cnt; i++) {
		size_t bitmap_size = t_info->regions[i].pg_cnt / 8;
		uint8_t *bitmap    = kmalloc(bitmap_size);
		memcpy(bitmap, t_info->regions[i].bitmap, bitmap_size);
		t_info->regions[i].bitmap = bitmap;
	}

	free_lower_half();

	uint32_t fb_pg_cnt = t_info->fb.pitch * t_info->fb.height / PAGE_SIZE;
	uint8_t *fb_data   = kmalloc_aligned_multi(fb_pg_cnt);
	for (uint32_t i = 0; i < fb_pg_cnt; i++) {
		map_page((vaddr_t)fb_data + i * PAGE_SIZE,
		         (vaddr_t)t_info->fb.bfr + i * PAGE_SIZE);
	}

	fb     = t_info->fb;
	fb.bfr = fb_data;

	reinit_console();
	fill_rect(0, 0, fb.width, fb.height - 1, 0x000000);
	logging_enabled = true;

	log(LOG_INFO, LOG_KERNEL, "Main kernel loaded and initialized!\n");

	for (;;) {
		__asm__ volatile("hlt");
	}
}
