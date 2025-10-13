#include "io.h"

void outb(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

uint8_t inb(uint16_t port) {
	uint8_t res;
	__asm__ volatile ("inb %w1, %b0" : "=a"(res) : "Nd"(port) : "memory");
	return res;
}

void io_wait(void) {
	outb(0x80, 0);
}
