#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "alloc.h"

extern uint32_t active_context;
extern struct context contexts[32];

void test() {
	kprint("Hello, world!\n");
	context_switch(contexts[0], false);
}

int main() {
	init_console();
	init_pic();
	init_idt();
	init_memory(&contexts[active_context]);

	struct context ctx = create_user_context(test, contexts[active_context]);
	context_switch(ctx, true);
	kprint("We're back!\n");

	// char *hello_world = kmalloc(13, contexts[active_context]);
	// hello_world[0] = 'H';
	// hello_world[1] = 'e';
	// hello_world[2] = 'l';
	// hello_world[3] = 'l';
	// hello_world[4] = 'o';
	// hello_world[5] = ' ';
	// hello_world[6] = 'w';
	// hello_world[7] = 'o';
	// hello_world[8] = 'r';
	// hello_world[9] = 'l';
	// hello_world[10] = 'd';
	// hello_world[11] = '!';
	// hello_world[12] = 0;
	// kprint(hello_world);

	for(;;) {
		__asm__("hlt");
	}
}
