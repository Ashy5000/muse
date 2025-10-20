#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"

int main() {
	init_console();
	init_memory();
	init_pic();
	init_idt();

	for(;;) {
		__asm__("hlt");
	}
}
