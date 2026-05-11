#include "pic.h"
#include "apic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "../drivers/hpet.h"
#include "pci.h"
#include "ata.h"
#include "userspace.h"
#include "syscall.h"
#include "alloc.h"

extern struct context *active_ctx;

void user_side() {
	uint32_t res = syscall(0x05, 0, 0, 0);
	if (res > 0) {
		kprint("syscall() errored! code: 0x");
		kprint_int(res, 16);
		kprint(".\n");
	}
	for (;;) {}
}

void kernel_side() {
	unlock_scheduler();
	struct block_header *header = active_ctx->heap;
	header->free = 3;
	header->size = 0x30000; // TODO: actually base this on something
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
	init_syscalls();

	register_ata();

	init_pci();

	start_hpet();

	lock_scheduler();
	create_context(idle, 1, false);
	create_context(kernel_side, 1, true);
	unlock_scheduler();

	for(;;) {
		__asm__("hlt");
	}
}
