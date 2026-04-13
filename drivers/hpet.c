#include <stdbool.h>
#include "text.h"
#include "hpet.h"

struct hpet *hpet_global;
void *hpet_base;
void *hpet_limit;
uint32_t timer_0_irq;

#define TICK_PERIOD 0xF4240 // 1 nanosecond

void init_hpet() {
	// Get the HPET SDT
	hpet_global = find_sdt("HPET");
	kprint("HPET at 0x");
	kprint_int((uintptr_t)hpet_global, 16);
	kprint(".\n");
	// Get the base address of the registers
	hpet_base = (void*)(uintptr_t)hpet_global->address.address;
	uint32_t *general_capabilities = hpet_base;
	uint8_t timer_count = ((general_capabilities[0] >> 8) & 0x1f) + 1;
	kprint("HPET supports ");
	kprint_int(timer_count, 10);
	kprint(" timers.\n");
	hpet_limit = hpet_base + 0x117 + 0x20 * (timer_count - 1);
	general_capabilities[1] = TICK_PERIOD;
	for (uint8_t i = 0; i < timer_count; i++) {
		uint32_t *timer_conf = (uint32_t*)(hpet_base + 0x100 + 0x20 * i);
		timer_conf[0] &= ~(1 << 3); // Nonperiodic
		timer_conf[0] |= 1 << 8;
	}
	if (timer_count >= 1) {
		// Map the first timer's IRQ
		uint32_t *timer_conf = (uint32_t*)(hpet_base + 0x100);
		uint32_t routing_map = timer_conf[1];
		for (uint32_t i = 0; i < 32; i++) {
			if (((routing_map >> i) & 1) > 0 && i != 0x0E) { // 0x0E is the IRQ for page faults
				timer_conf[0] |= (i << 9);
			}
		}
	}
}

void start_hpet() {
	uint32_t *general_conf = hpet_base + 0x10;
	general_conf[0] |= 1;
}

uint32_t get_time() {
	return *((uint32_t*)(hpet_base + 0xF0));
}

void init_delay(uint32_t delay) {
	uint32_t *timer_conf = (uint32_t*)(hpet_base + 0x100);
	uint32_t *comparator = (uint32_t*)(hpet_base + 0x108);
	uint32_t target = get_time() + delay;
	if (target % TICK_PERIOD > 0) {
		target += TICK_PERIOD - (target % TICK_PERIOD);
	}
	timer_conf[0] |= 1 << 2;
}
