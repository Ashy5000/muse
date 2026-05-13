#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_isr(void);
void init_syscalls();

#endif
