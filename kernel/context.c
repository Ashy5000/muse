#include "context.h"
#include "alloc.h"
#include "../drivers/text.h"
#include "apic.h"

struct context *active_ctx = 0;
struct context *next_ctx = 0;
struct context *last_ctx = 0;

struct context first_ctx;

extern void __attribute__((cdecl)) context_switch(struct context *ctx_new);

void create_kernel_context(func_ptr_t func_ptr) {
	struct context *ctx_new = kmalloc(sizeof(struct context), *active_ctx);
	ctx_new->heap = (void*)TASK_STACK_BASE + 1;
	ctx_new->present = true;
	ctx_new->page_directory = create_user_directory(*active_ctx);
	__asm__ volatile ("mov %0, %%cr3" :: "r"(ctx_new->page_directory) : );
	uint32_t* stack = (uint32_t*)TASK_STACK_BASE;
	stack[-1] = (uintptr_t)func_ptr;
	stack[-2] = 0; // EBX
	stack[-3] = 0; // ESI
	stack[-4] = 0; // EDI
	stack[-5] = TASK_STACK_BASE; // EBP
	__asm__ volatile ("mov %0, %%cr3" :: "r"(active_ctx->page_directory) : );
	ctx_new->esp = (uintptr_t)(stack - 5);
	ctx_new->next = 0;
	last_ctx->next = ctx_new;
	last_ctx = ctx_new;
}

void init_first_ctx() {
	active_ctx = &first_ctx;
	active_ctx->present = true;
	active_ctx->next = 0;
	next_ctx = active_ctx;
	last_ctx = active_ctx;
}

void schedule() {
	if (next_ctx) {
		struct context *ctx = next_ctx;
		next_ctx = next_ctx->next;
		context_switch(ctx);
	}
}

void preempt() {
	if (next_ctx) {
		struct context *ctx = next_ctx;
		last_ctx->next = next_ctx;
		last_ctx = last_ctx->next;
		next_ctx = next_ctx->next;
		last_ctx->next = 0;
		context_switch(ctx);
	}
}

void handle_timer_inner() {
	eoi();
	__asm__ volatile ("cli");
	preempt();
	__asm__ volatile ("sti");
}

__asm__ (
	".globl handle_timer;"
	"handle_timer:;"
	"pushal;"
	"cld;"
	"call handle_timer_inner;"
	"popal;"
	"iret;"
);
