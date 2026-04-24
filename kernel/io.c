#include "io.h"

void outb(uint16_t port, uint8_t val) {
	__asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port) : "memory");
}

uint8_t inb(uint16_t port) {
	uint8_t res;
	__asm__ volatile ("inb %w1, %b0" : "=a"(res) : "Nd"(port) : "memory");
	return res;
}

void outw(uint16_t port, uint16_t val) {
	__asm__ volatile ("outw %w0, %w1" :: "a"(val), "Nd"(port) : "memory");
}

uint16_t inw(uint16_t port) {
	uint16_t res;
	__asm__ volatile ("inw %w1, %w0" : "=a"(res) : "Nd"(port) : "memory");
	return res;
}

void outl(uint16_t port, uint32_t val) {
	__asm__ volatile ("outl %k0, %w1" :: "a"(val), "Nd"(port) : "memory");
}

uint32_t inl(uint16_t port) {
	uint32_t res;
	__asm__ volatile ("inl %w1, %k0" : "=a"(res) : "Nd"(port) : "memory");
	return res;
}

void io_wait(void) {
	outb(0x80, 0);
}
