#include "context.h"
#include "../drivers/text.h"

uint32_t active_context = 0;
struct context contexts[CTX_COUNT];

uint32_t create_user_context(func_ptr_t func_ptr, struct context ctx) {
	struct context ctx_new;
	ctx_new.instruction_ptr = (uintptr_t)func_ptr;
	ctx_new.ebp = USERSPACE_STACK_BASE;
	ctx_new.esp = USERSPACE_STACK_BASE;
	ctx_new.heap = (void*)USERSPACE_STACK_BASE + 1;
	ctx_new.present = 1;
	ctx_new.page_directory = create_user_directory(ctx);
	kprint("New page directory at p0x");
	kprint_int(ctx_new.page_directory, 16);
	kprint(".\n");
	int index = -1;
	for (int i = 0; i < CTX_COUNT; i++) {
		if (!contexts[i].present) {
			contexts[i] = ctx_new;
			index = i;
			break;
		}
	}
	return index;
}

void context_switch(uint32_t ctx_index_new) {
	contexts[active_context].instruction_ptr = (uintptr_t)&&ctx_switch_end;
	__asm__ volatile ("pushal");
	__asm__ volatile ("mov %%ebp, %0; mov %%esp, %1" : "=r"(contexts[active_context].ebp), "=r"(contexts[active_context].esp) :: );
	active_context = ctx_index_new;
	if (ctx_index_new > 0) {
		__asm__ volatile ("mov %0, %%cr3" :: "r"(contexts[active_context].page_directory) : );
	}
	__asm__ volatile ("mov %0, %%ebp; mov %1, %%esp" :: "r"(contexts[active_context].ebp), "r"(contexts[active_context].esp) : );
	__asm__ volatile ("jmp %0" :: "r"(contexts[active_context].instruction_ptr) : );
ctx_switch_end:
	__asm__ volatile ("popal");
}

void init_kernel_ctx() {
	contexts[0].present = true;
}
