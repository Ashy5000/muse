#include <stdio.h>
#include "syscall.h"

FILE *fopen(const char *restrict filename, const char *restrict mode) {
	syscall(1, (uintptr_t)filename, (uintptr_t)mode, 0);
	return 0;
}
