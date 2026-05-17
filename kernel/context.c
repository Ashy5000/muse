#include "context.h"
#include "alloc.h"
#include "../drivers/hpet.h"
#include "apic.h"
#include "../drivers/text.h"

struct context *active_ctx = 0;
struct context *next_ctx = 0;
struct context *last_ctx = 0;

struct context first_ctx;

extern void __attribute__((cdecl)) context_switch(struct context *ctx_new);

extern uint32_t tick_period;

void create_context(func_ptr_t func_ptr, uint8_t priority, bool user, struct scroll *first_scr) {
	lock_scheduler();
	struct context *ctx_new = kmalloc(sizeof(struct context));
	ctx_new->priority = priority;
	if (user) {
		ctx_new->heap = 0;
	} else {
		ctx_new->heap = (void*)0x10000;
	}
	ctx_new->present = true;
	ctx_new->page_directory = create_task_directory(func_ptr, user, first_scr);
	ctx_new->slices_remaining = 0;
	ctx_new->esp = (uintptr_t)(TASK_STACK_BASE - (5 * sizeof(uint32_t)));
	ctx_new->next = 0;
	ctx_new->first_scr = first_scr;
	if (next_ctx == 0) {
		next_ctx = ctx_new;
		last_ctx = ctx_new;
	} else {
		last_ctx->next = ctx_new;
		last_ctx = ctx_new;
	}
	unlock_scheduler();
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

struct context *first_sleeping_ctx = 0;

void handle_timer_inner() {
	eoi();
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

	if (active_ctx->slices_remaining == 0) {
		active_ctx->slices_remaining = active_ctx->priority;
	}
	active_ctx->slices_remaining--;
	if (active_ctx->slices_remaining > 0) {
		unlock_scheduler();
		return;
	}

	// Preempt this task
	preempt();

	uint32_t period_nano = tick_period / 1000000;
	unlock_scheduler();
	set_time(0);
	set_delay(1000000 / period_nano);
}

void sleep_millis(uint32_t millis) {
	lock_scheduler();
	active_ctx->next = first_sleeping_ctx;
	first_sleeping_ctx = active_ctx;
	first_sleeping_ctx->alarm = millis;
	schedule();
	unlock_scheduler();
}

void sleep_secs(uint32_t seconds) {
	sleep_millis(seconds * 1000);
}

void terminate() {
	kprint("Terminating process.\n");
	lock_scheduler();
	schedule();
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
