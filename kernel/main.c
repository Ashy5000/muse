#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "alloc.h"
#include "scroll.h"

extern struct context *active_ctx;
extern struct context contexts[32];

void test() {
	kprint("meow\n");
	context_switch(contexts);
}

int main() {
	init_console();
	init_pic();
	init_idt();
	init_kernel_ctx();
	init_memory(active_ctx);

	uint32_t index = create_user_context(test, *active_ctx);
	context_switch(contexts + index);
	kprint("woof\n");

	for(;;) {
		__asm__("hlt");
	}
}
