#include "userspace.h"
#include "context.h"
#include "../drivers/text.h"

extern struct context *active_ctx;

// TODO: Make data passing between bootloader and kernel better
uint32_t *tss = (uint32_t*)0x7d96;

func_ptr_t userspace_runway;

extern void jump_ring3(void);

void load_user_entry(func_ptr_t func_ptr) {
	userspace_runway = func_ptr;
}

// ONLY USE THIS FUNCTION TO INIT CTXS.
// It unlocks the scheduler when it runs.
void enter_ring3() {
	unlock_scheduler();
	tss[1] = TASK_STACK_BASE; // Doesn't matter if this overwrites data- this function never returns.
	kprint("Entering ring 3! Runway func at 0x");
	kprint_int((uintptr_t)userspace_runway, 16);
	kprint(".\n");
	jump_ring3();
	// TODO: Make unreachable/no return
}
