#include "io.h"

void outb(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

void io_wait(void) {
	outb(0x80, 0);
}
