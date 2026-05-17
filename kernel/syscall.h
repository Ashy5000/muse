#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

typedef void (*syscall_t)(uint32_t*);

void syscall_isr(void);
void init_syscalls();

#endif
