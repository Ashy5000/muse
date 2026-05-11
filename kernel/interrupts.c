#include <stdbool.h>
#include "interrupts.h"
#include "../drivers/text.h"
#include "../drivers/keyboard.h"
#include "context.h"
#include "syscall.h"

#include <stdint.h>

extern void handle_page_fault(void);
extern void syscall_isr(void);

#define ISR_COUNT 256

extern uint32_t timer_irq;

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

void handle_gpf(void) {
	kprint("KERNEL PANIC\n============\nA General Protection Fault (GPF) was detected.\n");
	__asm__ volatile ("cli; hlt");
}

void idt_set_entry(int vector, void *isr, bool user) {
	struct idt_entry *entry = idt + vector;
	entry->isr_addr_low = (uintptr_t)isr & 0xFFFF;
	entry->isr_addr_high = (uintptr_t)isr >> 16;
	entry->kernel_cs = 0x08;
	entry->attributes = 0x8E;
	if (user) {
		entry->attributes |= 0x60;
	}
	entry->reserved = 0;
}

void init_idt(void) {
	idt_set_entry(0xD, handle_gpf, false);
	idt_set_entry(0x31, handle_keypress, false);
	idt_set_entry(0x30 + timer_irq, handle_timer, false);
	idt_set_entry(0x80, syscall_isr, true);

	idtr_inst.base = (uintptr_t)idt;
	idtr_inst.limit = (uintptr_t)(sizeof(struct idt_entry)) * ISR_COUNT - 1;

	__asm__ volatile ("lidt %0" : : "m"(idtr_inst)); // Load IDT register
	__asm__ volatile ("sti");
}
