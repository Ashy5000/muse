#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

enum syscall_res {
	SYSCALL_SUCCESS,
	SYSCALL_INVALID_FN,
};

void syscall_isr(void);
enum syscall_res syscall(uint32_t fn, uint32_t arg0, uint32_t arg1, uint32_t arg2);
void init_syscalls();

#endif
