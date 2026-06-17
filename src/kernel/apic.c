#include <muse/alloc.h>
#include <muse/apic.h>
#include <muse/logging.h>
#include <muse/msr.h>
#include <muse/paging.h>
#include <muse/scroll.h>
#include <stdbool.h>
#include <stdint.h>

#define APIC_BASE_MSR 0x1B

uintptr_t apic_base;

extern uint32_t timer_irq;

// Set base address for APIC registers and enable local APIC.
void set_apic_base(uint32_t base) { setMSR(APIC_BASE_MSR, base | 0x800, 0); }

// Get base address for APIC registers.
uint32_t get_apic_base() {
	uint32_t lo, hi;
	getMSR(APIC_BASE_MSR, &lo, &hi);
	return lo & 0xFFFFF000;
}

void init_apic() {
	apic_base = get_apic_base();
	log(LOG_INFO, LOG_APIC, "APIC base registers at %x.\n", apic_base);
	set_apic_base(apic_base); /* This will keep the base the same, but
	                             enable the local APIC. */
	apic_base = (uintptr_t)map_phys_obj((void *)apic_base, 0x400);

	*((uint32_t *)(apic_base + 0xF0)) |=
	    0x100; /* Set bit 8 of the Spurious Intterupt Vector Register to
	              start receiving interrupts */
	log(LOG_INFO, LOG_APIC, "APIC initialized.\n");
}

void eoi() { *((uint32_t *)(uintptr_t)(apic_base + 0xB0)) = 0; }

void write_ioapic(uintptr_t base, uint8_t offset, uint32_t val) {
	*((volatile uint32_t *)base)          = offset;
	*((volatile uint32_t *)(base + 0x10)) = val;
}

uint32_t read_ioapic(uintptr_t base, uint8_t offset) {
	*((volatile uint32_t *)base) = offset;
	return *((volatile uint32_t *)base + 0x10);
}

void map_irq(uint32_t base, uint8_t irq) {
	uint32_t entry_low  = read_ioapic(base, 0x10 + (irq * 2));
	uint32_t entry_high = read_ioapic(base, 0x11 + (irq * 2));
	entry_low           = (entry_low & ~0xff) |
	                      ((0x30 + irq) & 0xff); // Set interrupt vector.
	entry_low &= ~(0x7 << 8);  // Set delivery mode to normal
	entry_low &= ~(0x1 << 11); // Physical
	entry_low &= ~(0x1 << 15); // Edge sensitive
	entry_low &= ~(0x1 << 16); // Don't mask the interrupt
	write_ioapic(base, 0x10 + (irq * 2), entry_low);
	write_ioapic(base, 0x11 + (irq * 2), entry_high);
}

void init_ioapic() {
	// Get the base address for the IOAPIC registers
	struct madt *madt = find_sdt("APIC");
	if (!madt) {
		log(LOG_ERROR, LOG_APIC,
		    "APIC info not present in ACPI tables!\n");
	}
	vaddr_t ioapic_base = 0;
	void *entry         = ((void *)madt) + 0x2C;
	while ((uintptr_t)entry < (uintptr_t)madt + madt->header.length) {
		uint8_t type = ((uint8_t *)entry)[0];
		if (type == 1) {
			// I/O APIC
			ioapic_base = *((uint32_t *)(entry + 4));
			break;
		}
		uint8_t length = ((uint8_t *)entry)[1];
		entry += length;
	}

	ioapic_base = (uintptr_t)map_phys_obj((void *)ioapic_base, 0x14);

	map_irq(ioapic_base, 1); // Keyboard
	map_irq(ioapic_base, timer_irq);
	log(LOG_INFO, LOG_APIC, "I/O APIC initialized.\n");
}
