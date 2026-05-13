#include "syscall.h"

void exit(int exit_code) {
	syscall(0, exit_code, 0, 0);
	__builtin_unreachable();
}
