#include <stdint.h>
#include <stdbool.h>
#include <cpuid.h>
#include "../drivers/text.h"
#include "msr.h"
#include "apic.h"
#include "paging.h"

#define APIC_BASE_MSR 0x1B

uint32_t apic_base;

extern uint32_t reserved_pages_count;
extern uint32_t reserved_pages[MAX_RESERVED_PAGES];

extern uint32_t timer_0_irq;

// Set base address for APIC registers and enable local APIC.
void set_apic_base(uint32_t base) {
	setMSR(APIC_BASE_MSR, base | 0x800, 0);
}

// Get base address for APIC registers.
uint32_t get_apic_base() {
	uint32_t lo, hi;
	getMSR(APIC_BASE_MSR, &lo, &hi);
	return lo & 0xFFFFF000;
}

void init_apic() {
	apic_base = get_apic_base();
	kprint("APIC base registers at 0x");
	kprint_int(apic_base, 16);
	kprint(".\n");
	set_apic_base(apic_base); // This will keep the base the same, but enable the local APIC.
	*((uint32_t*)(uintptr_t)(apic_base + 0xF0)) |= 0x100; // Set bit 8 of the Spurious Intterupt Vector Register to start receiving interrupts
	uint32_t apic_limit = apic_base + 0x400;
	uint32_t apic_pages_start = apic_base - (apic_base % PAGE_SIZE);
	uint32_t apic_pages_end = apic_limit;
	if (apic_pages_end % PAGE_SIZE > 0) {
		apic_pages_end += PAGE_SIZE - (apic_pages_end % PAGE_SIZE);
	}
	for (uint32_t i = 0; i < (apic_pages_end - apic_pages_start) / PAGE_SIZE; i++) {
		reserved_pages[reserved_pages_count] = apic_pages_start + (i * PAGE_SIZE);
		reserved_pages_count++;
	}
}

void eoi() {
	*((uint32_t*)(uintptr_t)(apic_base + 0xB0)) = 0;
}

void write_ioapic(uint32_t base, uint8_t offset, uint32_t val) {
	*((volatile uint32_t*)(uintptr_t)base) = offset;
	*((volatile uint32_t*)(uintptr_t)(base + 0x10)) = val;
}

uint32_t read_ioapic(uint32_t base, uint8_t offset) {
	*((volatile uint32_t*)(uintptr_t)base) = offset;
	return *((volatile uint32_t*)(uintptr_t)(base + 0x10));
}

void map_irq(uint32_t base, uint8_t irq) {
	uint32_t entry_low = read_ioapic(base, 0x10 + (irq * 2));
	uint32_t entry_high = read_ioapic(base, 0x11 + (irq * 2));
	entry_low = (entry_low & ~0xff) | ((0x30 + irq) & 0xff); // Set interrupt vector.
	entry_low &= ~(0x7 << 8); // Set delivery mode to normal
	entry_low &= ~(0x1 << 11); // Physical
	entry_low &= ~(0x1 << 15); // Edge sensitive
	entry_low &= ~(0x1 << 16); // Don't mask the interrupt
	write_ioapic(base, 0x10 + (irq * 2), entry_low);
	write_ioapic(base, 0x11 + (irq * 2), entry_high);
}

void init_ioapic() {
	// Get the base address for the IOAPIC registers
	struct madt *madt = find_sdt("APIC");
	uint32_t ioapic_base;
	void *entry = ((void*)madt) + 0x2C;
	while ((uintptr_t)entry < (uintptr_t)madt + madt->header.length) {
		uint8_t type = ((uint8_t*)entry)[0];
		if (type == 1) {
			// I/O APIC
			ioapic_base = *((uint32_t*)(entry + 4));
			break;
		}
		uint8_t length = ((uint8_t*)entry)[1];
		entry += length;
	}

	// Get the active local APIC ID
	unsigned int ebx, unused;
	__get_cpuid(1, &unused, &ebx, &unused, &unused);
	uint8_t apic_id = (ebx >> 24) & 0xff;
	kprint("Local APIC ID is ");
	kprint_int(apic_id, 10);
	kprint(".\n");

	uint32_t max_redirection_entries = (read_ioapic(ioapic_base, 0x01) >> 16) & 0xff;

	map_irq(ioapic_base, 1); // Keyboard
	map_irq(ioapic_base, timer_0_irq);
}
