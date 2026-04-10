#include "context.h"
#include "../drivers/text.h"

struct context *active_ctx;
struct context contexts[CTX_COUNT];

extern void __attribute__((cdecl)) context_switch(struct context *ctx_new);

uint32_t create_user_context(func_ptr_t func_ptr, struct context ctx) {
	struct context ctx_new;
	ctx_new.heap = (void*)USERSPACE_STACK_BASE + 1;
	ctx_new.present = 1;
	ctx_new.page_directory = create_user_directory(ctx);
	__asm__ volatile ("mov %0, %%cr3" :: "r"(ctx_new.page_directory) : );
	uint32_t* stack = (uint32_t*)USERSPACE_STACK_BASE;
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0; // EBX
	stack[-3] = 0; // ESI
	stack[-4] = 0; // EDI
	stack[-5] = USERSPACE_STACK_BASE; // EBP
	__asm__ volatile ("mov %0, %%cr3" :: "r"(active_ctx->page_directory) : );
	ctx_new.esp = (uintptr_t)(stack - 5);
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

void init_kernel_ctx() {
	active_ctx = contexts;
	active_ctx->present = true;
}
