#include "syscall.h"
#include "context.h"
#include "../drivers/text.h"

syscall_t syscalls[16];

void handle_syscall(uint32_t *args) {
	uint32_t fn = args[0];
	if (fn > 15) {
		return;
	}
	if (syscalls[fn]) {
		syscalls[fn](args + 1);
	}
}

void syscall_exit(uint32_t *args) {
	kprint("Exiting with code ");
	kprint_int(args[0], 10);
	kprint(".\n");
	terminate();
}

void init_syscalls() {
	syscalls[0] = syscall_exit;
}
