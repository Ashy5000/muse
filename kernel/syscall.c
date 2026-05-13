#include "syscall.h"
#include "memory.h"
#include "../drivers/text.h"

func_ptr_t syscalls[16];

void handle_syscall(uint32_t *args) {
	uint32_t fn = args[0];
	if (fn > 15) {
		return;
	}
	if (syscalls[fn]) {
		syscalls[fn]();
	}
}

void syscall_exit() {
	kprint("user process called exit()!");
	__asm__ volatile ("cli; hlt");
}

void init_syscalls() {
	syscalls[0] = syscall_exit;
}
