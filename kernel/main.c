#include "pic.h"
#include "apic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "../drivers/hpet.h"
#include "pci.h"

extern struct context *active_ctx;

void idle() {
	unlock_scheduler();
	for (;;) {
		lock_scheduler();
		preempt();
		unlock_scheduler();
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
	init_pci();

	start_hpet();

	lock_scheduler();
	create_kernel_context(idle, 1);
	unlock_scheduler();

	for(;;) {
		__asm__("hlt");
	}
}
