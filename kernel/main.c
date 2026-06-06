#include "../drivers/hpet.h"
#include "../drivers/text.h"
#include "apic.h"
#include "context.h"
#include "elf.h"
#include "interrupts.h"
#include "logging.h"
#include "pci.h"
#include "pic.h"
#include "syscall.h"
#include "term.h"
#include "userspace.h"

extern struct context *active_ctx;

void idle() {
	for (;;) {
		preempt();
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
	init_userspace();
	init_scheduler();
	init_memory();
	init_syscalls();
	init_root_term();

	register_ata();

	init_pci();

	start_hpet();

	log(LOG_INFO, LOG_KERNEL,
	    "All essential systems loaded. Welcome to muse!\n");

	load_elf("/ext2/bin/test.o", 0, 0, root_term, root_term, root_term);

	preempt();

	lock_scheduler();
	create_context(idle, 1, false, 0, 0, root_term, root_term, root_term);
	unlock_scheduler();

	for (;;) {
		__asm__("hlt");
	}
}
