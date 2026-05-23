#ifndef _SYSCALL_H
#define _SYSCALL_H 1

#include <stdint.h>

int muse_syscall(uint32_t fn, uint32_t arg0, uint32_t arg1, uint32_t arg2);

#endif
