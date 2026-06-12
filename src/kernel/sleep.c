#include <muse/context.h>
#include <muse/scheduler.h>
#include <muse/sleep.h>

struct context *first_sleeping_ctx;

extern struct context *last_ctx;
extern struct context *active_ctx;

void sleep_tick() {
	struct context *previous_ctx = 0;
	struct context *sleeping_ctx = first_sleeping_ctx;
	while (sleeping_ctx != 0) {
		sleeping_ctx->alarm--;
		if (sleeping_ctx->alarm == 0) {
			last_ctx->next = sleeping_ctx;
			last_ctx       = sleeping_ctx;
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
}

void sleep_millis(uint32_t millis) {
	lock_scheduler();
	active_ctx->next          = first_sleeping_ctx;
	first_sleeping_ctx        = active_ctx;
	first_sleeping_ctx->alarm = millis;
	schedule();
	unlock_scheduler();
}

void sleep_secs(uint32_t seconds) { sleep_millis(seconds * 1000); }
