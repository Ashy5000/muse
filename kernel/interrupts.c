#include "interrupts.h"
#include "io.h"
#include "../drivers/text.h"
#include "../drivers/keyboard.h"

#include <stdint.h>

#define ISR_COUNT 256

struct __attribute__((packed)) idt_entry {
	uint16_t isr_addr_low;
	uint16_t kernel_cs;
	uint8_t reserved;
	uint8_t attributes;
	uint16_t isr_addr_high;
};

struct idt_entry idt[256];

// To be stored in the IDTR register
struct __attribute__((packed)) idtr {
	uint16_t limit; // Length of IDT - 1
	uint32_t base; // Address of IDT
};

struct idtr idtr_inst;

void handle_exception(void) {
	kprint("\n\n\nCritical exception: kernel will exit.\n");
	__asm__ volatile ("cli; hlt");
}

void *isrs[ISR_COUNT];

void idt_set_entry(int vector, void *isr) {
	struct idt_entry *entry = idt + vector;
	entry->isr_addr_low = (uintptr_t)isr & 0xFFFF;
	entry->isr_addr_high = (uintptr_t)isr >> 16;
	entry->kernel_cs = 0x08;
	entry->attributes = 0x8E;
	entry->reserved = 0;
}

void init_idt(void) {
	for (int i = 0; i < 32; i++) {
		isrs[i] = handle_exception;
	}
	isrs[0x21] = handle_keypress;

	idtr_inst.base = (uintptr_t)idt;
	idtr_inst.limit = (uintptr_t)(sizeof(struct idt_entry)) * ISR_COUNT - 1;

	for (int i = 0; i < ISR_COUNT; i++) {
		idt_set_entry(i, isrs[i]);
	}

	__asm__ volatile ("lidt %0" : : "m"(idtr_inst)); // Load IDT register
	__asm__ volatile ("sti");
}
