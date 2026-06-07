#ifndef MUSEBOOT_H
#define MUSEBOOT_H

#include <stdint.h>

struct smap_entry {
	uint32_t addr_lo;
	uint32_t addr_hi;
	uint32_t size_lo;
	uint32_t size_hi;
	uint32_t type;
	uint32_t acpi;
};

#endif
