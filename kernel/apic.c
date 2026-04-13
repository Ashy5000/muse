#include <stdint.h>
#include <stdbool.h>
#include "../drivers/text.h"
#include "msr.h"
#include "apic.h"

#define APIC_BASE_MSR 0x1B

// Set base address for APIC registers and enable local APIC.
void set_apic_base(uint32_t base) {
	setMSR(APIC_BASE_MSR, ((base & 0xFFFF0000) | 0x800), 0);
}

// Get base address for APIC registers.
uint32_t get_apic_base() {
	uint32_t lo, hi;
	getMSR(APIC_BASE_MSR, &lo, &hi);
	return lo & 0xFFFFF000;
}

void init_apic() {
	set_apic_base(get_apic_base()); // This will keep the base the same, but enable the local APIC.
	*((uint32_t*)0xFEE000F0) |= 0x100; // Set bit 8 of the Spurious Intterupt Vector Register to start receiving interrupts
}

void init_ioapic() {
	struct madt *madt = find_sdt("APIC");
	uint32_t ioapic_base;
	void *entry = ((void*)madt) + 0x2C;
	while ((uintptr_t)entry < (uintptr_t)madt + madt->header.length) {
		uint8_t type = ((uint8_t*)entry)[0];
		kprint("Type: ");
		kprint_int(type, 10);
		kprint(".\n");
		if (type == 1) {
			// I/O APIC
			ioapic_base = *((uint32_t*)(entry + 4));
			break;
		}
		uint8_t length = ((uint8_t*)entry)[1];
		entry += length;
	}
	kprint("I/O APIC address: 0x");
	kprint_int(ioapic_base, 16);
	kprint(".\n");
}
