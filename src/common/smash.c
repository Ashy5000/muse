#include <muse/logging.h>
#include <stdint.h>

uintptr_t __stack_chk_guard = 0xDEADBEEF;

__attribute__((noreturn)) void __stack_chk_fail(void) {
	log(LOG_ERROR, LOG_KERNEL,
	    "Stack smashing detected! There must be a bug somewhere. Your next "
	    "step should be using your debugger to give a backtrace.\n");
	__asm__ volatile("cli; hlt");
	__builtin_unreachable();
}
