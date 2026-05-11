#include "syscall.h"
#include "memory.h"

func_ptr_t syscalls[16];

void handle_syscall(uint32_t *args) {
	uint32_t fn = args[0];
	if (fn > 15) {
		args[0] = SYSCALL_INVALID_FN;
		return;
	}
	if (syscalls[fn]) {
		args[0] = SYSCALL_SUCCESS;
		syscalls[fn]();
		return;
	}
	args[0] = SYSCALL_INVALID_FN;
}

enum syscall_res syscall(uint32_t fn, uint32_t arg0, uint32_t arg1, uint32_t arg2) {
	__asm__ volatile ("mov %0, %%eax;"
			"mov %1, %%ebx;"
			"mov %2, %%ecx;"
			"int $0x80"
			:: "g"(fn), "g"(arg0), "g"(arg1), "g"(arg2)
			: "%eax"); // ebx and ecx are preserved
	uint32_t eax;
	__asm__ volatile ("mov %%eax, %0" : "=g"(eax) :: );
	return eax;
}

void init_syscalls() {
	// This function will fill the syscall table with fns
}
