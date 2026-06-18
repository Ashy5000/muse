#include <muse/apic.h>
#include <muse/context.h>
#include <muse/hpet.h>
#include <muse/logging.h>
#include <muse/paging.h>
#include <muse/scheduler.h>
#include <muse/sleep.h>
#include <stdbool.h>

struct hpet *hpet_global;
struct scroll hpet_scr;
void *hpet_base;
uint32_t timer_idx;
uint32_t timer_irq;
uint32_t tick_period;

void start_hpet() {
	uint32_t *general_conf = hpet_base + 0x10;
	general_conf[0] |= 1;
}

void stop_hpet() {
	uint32_t *general_conf = hpet_base + 0x10;
	general_conf[0] &= ~1;
}

void init_hpet() {
	// Get the HPET SDT
	hpet_global = find_sdt("HPET");
	log(LOG_INFO, LOG_HPET, "Found HPET SDT at %x.\n",
	    (uintptr_t)hpet_global);

	// Get the base address of the registers
	void *base_old = (void *)(uintptr_t)hpet_global->address.address;
	hpet_base      = map_phys_obj(base_old, 0x117);

	uint32_t *general_capabilities = hpet_base;
	uint8_t timer_count = ((general_capabilities[0] >> 8) & 0x1f) + 1;

	unmap_page((vaddr_t)hpet_base);
	kfree(hpet_base);

	hpet_base = map_phys_obj(base_old, 0x117 + 0x20 * timer_count);

	general_capabilities = hpet_base;
	tick_period          = general_capabilities[1];

	stop_hpet();

	for (uint8_t i = 0; i < timer_count; i++) {
		uint32_t *timer_conf =
		    (uint32_t *)(hpet_base + 0x100 + 0x20 * i);

		timer_conf[0] |= 1 << 8; // 32-bit mode

		uint32_t routing_map = timer_conf[1];
		for (uint32_t j = 0; j < 32; j++) {
			if (((routing_map >> j) & 1) > 0 &&
			    j != 0x01) { // 0x01 is the keyboard IRQ
				log(LOG_DEBUG, LOG_HPET,
				    "Mapping timer %x -> IRQ %x", i, j);
				timer_irq = j;
				timer_conf[0] |= timer_irq << 9; // Set IRQ
				timer_conf[0] |= 1 << 2; // Enable interrupts
				timer_idx = i;
				return;
			}
		}
	}
}

uint32_t get_time() { return *((uint32_t *)(hpet_base + 0xF0)); }

void set_time(uint32_t time) {
	stop_hpet();
	*((uint32_t *)(hpet_base + 0xF0)) = time;
	start_hpet();
}

void set_delay(uint32_t delay) {
	volatile uint32_t *comparator = (uint32_t *)(hpet_base + 0x108);
	comparator[0]                 = delay;
}

extern struct context *active_ctx;
extern struct context *last_ctx;

void handle_timer_inner() {
	eoi();
	lock_scheduler();

	// Update sleeping contexts
	sleep_tick();

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

__asm__(".globl handle_timer;"
        "handle_timer:;"
        "pushal;"
        "cld;"
        "call handle_timer_inner;"
        "popal;"
        "iret;");
