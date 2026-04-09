#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "alloc.h"
#include "scroll.h"

extern uint32_t active_context;
extern struct context contexts[32];

void test() {
	kprint("Meow!\n");
	__asm__ volatile ("cli; hlt");
}

int main() {
	init_console();
	init_pic();
	init_idt();
	init_memory(&contexts[active_context]);
	init_kernel_ctx();

	struct scroll scr_0 = kmalloc_page(contexts[active_context]);
	struct scroll scr_1 = kmalloc_page(contexts[active_context]);
	kprint("Created scrolls mapping p0x");
	kprint_int(scr_0.aligned_backend.page, 16);
	kprint(" and p0x");
	kprint_int(scr_1.aligned_backend.page, 16);
	kprint(".\n");


	for(;;) {
		__asm__("hlt");
	}
}
