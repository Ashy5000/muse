#ifndef APIC_H
#define APIC_H

#include <stdbool.h>
#include "acpi.h"

struct madt {
	struct acpi_sdt_header header;
	uint32_t local_apic_address;
	uint32_t flags;
	void *entries;
};

void init_apic();
void eoi();
void init_ioapic();

#endif
