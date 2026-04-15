#include "pic.h"
#include "apic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "../drivers/hpet.h"

extern struct context *active_ctx;

void test() {
	__asm__ volatile ("sti");
	for(;;) {
		kprint("b");
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
	start_hpet();
	
	create_kernel_context(test);

	for(;;) {
		kprint("a");
	}

	for(;;) {
		__asm__("hlt");
	}
}
