#include "syscall.h"
#include <unistd.h>

void *sbrk(intptr_t increment) {
	return (void *)(uintptr_t)muse_syscall(1, increment, 0, 0);
}
