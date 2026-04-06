#include "context.h"
#include "alloc.h"
#include "scroll.h"
#include "../drivers/text.h"

extern void get_instruction_pointer(void);

uint32_t active_context = 0;
uint32_t contexts_count = 1;
struct context contexts[32];

struct context create_user_context(func_ptr_t func_ptr, struct context ctx) {
	struct context ctx_new;

	ctx_new.page_directory = create_user_directory(ctx);

	__asm__ volatile ("mov %0, %%cr3" :: "r"(ctx_new.page_directory) : "memory" );
	*((func_ptr_t*)(uintptr_t)USERSPACE_STACK_BASE - 1) = func_ptr;
	__asm__ volatile ("mov %0, %%cr3" :: "r"(contexts[active_context].page_directory) : "memory" );
	ctx_new.ebp = USERSPACE_STACK_BASE;
	ctx_new.esp = USERSPACE_STACK_BASE - sizeof(func_ptr_t);
	ctx_new.heap = (void*)(uintptr_t)USERSPACE_STACK_BASE + 1;
	return ctx_new;
}

// TODO: find room for context in contexts array.
void context_switch(struct context new_context, bool kernel) {
	// Push registers onto the old stack
	__asm__ volatile ("pushal");
	// Push instruction pointer onto the old stack
	__asm__ goto volatile ("mov %0, %%ebx; push %%ebx" :::: switch_end);
	__asm__ volatile ("mov %%ebp, %0; mov %%esp, %1" : "=r"(contexts[active_context].ebp), "=r"(contexts[active_context].esp) :: "memory");

	// Move the stack pointers
	__asm__ volatile ("mov %0, %%ebp; mov %1, %%esp" :: "r"(new_context.ebp), "r"(new_context.esp) : "memory" );

	if (kernel) {
		// Load new paging structures
		__asm__ volatile ("mov %0, %%cr3" :: "r"(new_context.page_directory) : "memory" );

		// Pop instruction pointer off of the stack and make the switch!
		__asm__ volatile ("pop %ebx;"
				  "jmp %ebx");
	} else {
		// Load new paging structures
		__asm__ volatile ("mov %0, %%cr3" :: "r"(new_context.page_directory) : "memory" );

		// Pop instruction pointer off of the stack and make the switch!
		__asm__ volatile ("pop %ebx;"
				  "jmp %ebx");
	}

switch_end:
	// We're back! Let's restore the registers.
	__asm__ volatile ("popal");
}
