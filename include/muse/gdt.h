#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_descriptor {
	uint16_t size_dec;
	uintptr_t start;
} __attribute__((packed));

struct gdt_segment_descriptor {
	uint16_t limit_lo;
	unsigned int base_lo : 24;
	uint8_t access_byte;
	unsigned int limit_hi : 4;
	unsigned int flags : 4;
	uint8_t base_hi;
} __attribute__((packed));

#endif
