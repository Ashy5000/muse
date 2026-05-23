#include "syscall.h"

extern void _fini(void);

__attribute__ ((noreturn)) void exit(int exit_code) {
	_fini();
	muse_syscall(0, exit_code, 0, 0);
	__builtin_unreachable();
}
