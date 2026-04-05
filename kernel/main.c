#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"
#include "context.h"
#include "paging.h"

extern uint32_t active_context;
extern struct context contexts[32];

// void test(void) {
// 	kprint("this is a test.\n");
// }

int main() {
	init_console();
	init_pic();
	init_idt();
	init_memory(&contexts[active_context]);
	map_page(1024 * 1024 * 2, 1024 * 1024 * 2);
	// kprint("TEST");

	// vaddr_t phys_addr = (uintptr_t)kpage_alloc();
	// paddr_t virt_addr = 0x1000000;
	// map_page(virt_addr, phys_addr, 0);
	// if (!check_page_status(virt_addr)) {
	// 	kprint("Paging failed!\n");
	// }
	// char *str = kmalloc(6, 0, contexts[active_context]);
	// char *str = (char*)(uintptr_t)virt_addr;
	// str[0] = 'h';
	// str[1] = 'e';
	// str[2] = 'l';
	// str[3] = 'l';
	// str[4] = 'o';
	// str[5] = 0;
	// kprint(str);
	// struct context test_context = create_user_context(&test);
	// context_switch(test_context);

	for(;;) {
		__asm__("hlt");
	}
}
