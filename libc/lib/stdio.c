#include <stdio.h>
#include "syscall.h"

FILE *fopen(const char *restrict filename, const char *restrict mode) {
	muse_syscall(2, (uintptr_t)filename, (uintptr_t)mode, 0);
	return 0;
}
