#include "syscall.h"

int muse_syscall(uint32_t fn, uint32_t arg0, uint32_t arg1, uint32_t arg2) {
	__asm__ volatile("mov %0, %%eax;"
	                 "mov %1, %%ebx;"
	                 "mov %2, %%ecx;"
	                 "mov %3, %%edx;"
	                 "int $0x80" ::"g"(fn),
	                 "g"(arg0), "g"(arg1), "g"(arg2)
	                 : "%eax", "%ebx", "%ecx");
	uint32_t eax;
	__asm__ volatile("mov %%eax, %0" : "=g"(eax)::);
	return eax;
}
