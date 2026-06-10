#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

typedef uint32_t (*syscall_t)(uint32_t*);

void syscall_isr(void);
void init_syscalls();

#endif
