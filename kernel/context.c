#include "context.h"
#include "alloc.h"
#include "../drivers/hpet.h"
#include "../drivers/text.h"
#include "apic.h"

struct context *active_ctx = 0;
struct context *next_ctx = 0;
struct context *last_ctx = 0;

struct context first_ctx;

extern void __attribute__((cdecl)) context_switch(struct context *ctx_new);

void create_kernel_context(func_ptr_t func_ptr, uint8_t priority) {
	struct context *ctx_new = kmalloc(sizeof(struct context), *active_ctx);
	ctx_new->heap = (void*)TASK_STACK_BASE + 1;
	ctx_new->present = true;
	ctx_new->page_directory = create_user_directory(*active_ctx);
	ctx_new->priority = priority;
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
	if (next_ctx == 0) {
		next_ctx = ctx_new;
		last_ctx = ctx_new;
	} else {
		last_ctx->next = ctx_new;
		last_ctx = ctx_new;
	}
}

void init_first_ctx() {
	active_ctx = &first_ctx;
	active_ctx->present = true;
	active_ctx->next = 0;
	active_ctx->priority = 1;
}

uint32_t irq_disable_counter = 0;

void lock_scheduler() {
	__asm__ volatile ("cli");
	irq_disable_counter++;
}

void unlock_scheduler() {
	irq_disable_counter--;
	if (irq_disable_counter == 0) {
		__asm__ volatile ("sti");
	}
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
		active_ctx->next = 0;
		last_ctx->next = active_ctx;
		last_ctx = last_ctx->next;
		next_ctx = next_ctx->next;
		context_switch(ctx);
	}
}

uint16_t acc;
struct context *first_sleeping_ctx = 0;

#define SECOND 781248
#define FREQ_DIV (1 << 7)

void handle_timer_inner() {
	eoi();
	set_time(0);
	set_delay(SECOND * active_ctx->priority);
	acc++;
	if ((acc % FREQ_DIV) > 0) {
		return;
	}

	lock_scheduler();

	// Update sleeping contexts
	struct context *previous_ctx = 0;
	struct context *sleeping_ctx = first_sleeping_ctx;
	while(sleeping_ctx != 0) {
		sleeping_ctx->alarm--;
		if (sleeping_ctx->alarm == 0) {
			last_ctx->next = sleeping_ctx;
			last_ctx = sleeping_ctx;
			if (previous_ctx == 0) {
				first_sleeping_ctx = sleeping_ctx->next;
			} else {
				previous_ctx->next = sleeping_ctx->next;
			}
			sleeping_ctx->next = 0;
			// TODO: Preempt based on priority
		}
		previous_ctx = sleeping_ctx;
		sleeping_ctx = sleeping_ctx->next;
	}

	// Preempt this task
	preempt();

	unlock_scheduler();
}

void sleep_secs(uint32_t seconds) {
	lock_scheduler();
	active_ctx->next = first_sleeping_ctx;
	first_sleeping_ctx = active_ctx;
	first_sleeping_ctx->alarm = seconds;
	schedule();
	unlock_scheduler();
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
