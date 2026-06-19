#define KERNEL

#include <muse/alloc.h>
#include <muse/apic.h>
#include <muse/ata.h>
#include <muse/context.h>
#include <muse/elf_user.h>
#include <muse/gdt.h>
#include <muse/hpet.h>
#include <muse/interrupts.h>
#include <muse/keyboard.h>
#include <muse/logging.h>
#include <muse/pci.h>
#include <muse/pic.h>
#include <muse/ps2.h>
#include <muse/psf.h>
#include <muse/scheduler.h>
#include <muse/syscall.h>
#include <muse/term.h>
#include <muse/text.h>
#include <muse/trampoline.h>
#include <muse/userspace.h>

extern void *rsdt_global;
extern bool logging_enabled;
extern struct vga_framebuffer fb;
extern uint32_t *tss;

struct trampoline_info *t_info = 0;
struct gdt_descriptor gdt_desc;

#define KERNEL_ALIGNED_HEAP_SIZE (1024 * PAGE_SIZE)

void idle() {
	for (;;) {
		preempt();
	}
}

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

	fb     = t_info->fb;
	fb.bfr = map_phys_obj(fb.bfr, fb.pitch * fb.height);

	reinit_console();
	fill_rect(0, 0, fb.width, fb.height, 0x000000);
	logging_enabled = true;

	struct gdt_descriptor desc_old;
	__asm__("sgdtl %0" : "=m"(desc_old)::);
	gdt_desc.size_dec = desc_old.size_dec;
	void *gdt         = kmalloc(gdt_desc.size_dec + 1);
	gdt_desc.start    = (uintptr_t)gdt;
	memcpy(gdt, (void *)desc_old.start, gdt_desc.size_dec + 1);
	size_t offset = 0;
	for (; offset < (size_t)gdt_desc.size_dec + 1;
	     offset += sizeof(struct gdt_segment_descriptor)) {
		struct gdt_segment_descriptor *seg_desc = gdt + offset;
		if (seg_desc->access_byte & 0x10) {
			continue;
		}
		uint8_t type = seg_desc->access_byte & 0xF;
		if (type != 0x9 && type != 0xB) {
			continue;
		}
		uint32_t *tss_old =
		    (uint32_t *)(uintptr_t)((seg_desc->base_hi << 24) |
		                            seg_desc->base_lo);
		tss = kmalloc(0x6C);
		memcpy(tss, tss_old, 0x6C);
		seg_desc->base_hi = (uintptr_t)tss >> 24;
		seg_desc->base_lo = (uintptr_t)tss & 0xFFFFFF;
		break;
	}
	if (!tss) {
		log(LOG_ERROR, LOG_KERNEL, "Could not find TSS!\n");
		__asm__ volatile("cli; hlt");
	}

	__asm__("lgdtl %0" ::"m"(gdt_desc) :);
	__asm__ volatile("ltr %0" ::"r"(offset) :);

	free_lower_half();

	log(LOG_INFO, LOG_KERNEL, "Main kernel loaded and initialized!\n");

	reinit_acpi();
	init_ps2();
	init_keyboard();

	init_hpet();
	init_idt();
	init_pic();
	init_apic();
	init_ioapic();

	register_ata();
	init_pci();

	init_userspace();
	init_first_ctx();
	init_syscalls();
	init_root_term();
	lock_scheduler();
	create_context(idle, 1, false, 0, 0, root_term, root_term, root_term);
	unlock_scheduler();
	load_elf_user("/ext2/bin/test.o", 0, 0, root_term, root_term,
	              root_term);
	log(LOG_INFO, LOG_KERNEL, "Loaded mused.\n");

	for (;;) {
		__asm__ volatile("hlt");
	}
}
