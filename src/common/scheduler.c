#include <muse/apic.h>
#include <muse/context.h>
#include <muse/hpet.h>
#include <muse/scheduler.h>

extern struct context *active_ctx;
extern struct context *next_ctx;
extern struct context *last_ctx;

extern struct context first_ctx;

extern uint32_t ctx_id;

extern uint32_t tick_period;

uint32_t irq_disable_counter = 0;

void lock_scheduler() {
	__asm__ volatile("cli");
	irq_disable_counter++;
}

void unlock_scheduler() {
	irq_disable_counter--;
	if (irq_disable_counter == 0) {
		__asm__ volatile("sti");
	}
}

void schedule() {
	if (next_ctx) {
		struct context *ctx = next_ctx;
		next_ctx            = next_ctx->next;
		context_switch(ctx);
	}
}

void preempt() {
	if (next_ctx) {
		struct context *ctx = next_ctx;
		active_ctx->next    = 0;
		last_ctx->next      = active_ctx;
		last_ctx            = last_ctx->next;
		next_ctx            = next_ctx->next;
		context_switch(ctx);
	}
}

void terminate() { schedule(); }
