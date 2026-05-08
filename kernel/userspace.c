#include "userspace.h"
#include "context.h"
#include "../drivers/text.h"

extern struct context *active_ctx;

// TODO: Make data passing between bootloader and kernel better
uint32_t *tss = (uint32_t*)0x7d96;

func_ptr_t userspace_runway;

extern void jump_ring3(void);

void enter_ring3(func_ptr_t func_ptr) {
	tss[1] = TASK_STACK_BASE; // Doesn't matter if this overwrites data- this function never returns.
	// __asm__ volatile ("mov $0x28, %%ax; ltr %%ax" ::: "%ax");
	userspace_runway = func_ptr;
	jump_ring3();
	// __builtin_unreachable();
}
