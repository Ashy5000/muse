#define KERNEL

#include <muse/trampoline.h>

struct trampoline_info *t_info = 0;

int kmain() {
	for (;;) {
		__asm__ volatile("hlt");
	}
}
