#include <muse/paging.h>
#include <muse/msr.h>

void setMSR(uint32_t msr, uint32_t lo, uint32_t hi) {
	__asm__ volatile ("wrmsr" :: "a"(lo), "d"(hi), "c"(msr) : );
}

void getMSR(uint32_t msr, uint32_t *lo, uint32_t *hi) {
	__asm__ volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr) : );
}
