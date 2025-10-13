#include "memory.h"
#include "pic.h"
#include "interrupts.h"
#include "../drivers/text.h"

int main() {
	init_memory();
	init_pic();
	init_idt();

	while (1) {}
}
