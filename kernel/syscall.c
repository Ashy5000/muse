#include "syscall.h"
#include "context.h"

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

void syscall_exit(__attribute__((unused)) uint32_t *args) {
	terminate();
}

void init_syscalls() {
	syscalls[0] = syscall_exit;
}
