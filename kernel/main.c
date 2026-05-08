#include "pic.h"
#include "apic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "../drivers/hpet.h"
#include "pci.h"
#include "ata.h"
#include "userspace.h"

extern struct context *active_ctx;

void user_side() {
	for (;;) {
		kput_char(' ');
	}
}

void kernel_side() {
	unlock_scheduler();
	enter_ring3(user_side);
	for (;;) {}
}

void idle() {
	unlock_scheduler();
	for (;;) {
		lock_scheduler();
		preempt();
		unlock_scheduler();
	}
}

void rando_task() {
	unlock_scheduler();
	for (;;) {
		kput_char('#');
	}
}

int main() {
	init_console();
	init_acpi();
	init_hpet();
	init_pic();
	init_apic();
	init_ioapic();
	init_idt();
	init_first_ctx();
	init_memory(active_ctx);

	register_ata();

	init_pci();

	start_hpet();

	lock_scheduler();
	create_context(idle, 1, false);
	create_context(rando_task, 1, false);
	create_context(kernel_side, 1, true);
	unlock_scheduler();

	for(;;) {
		__asm__("hlt");
	}
}
