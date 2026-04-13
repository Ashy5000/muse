#ifndef HPET_H
#define HPET_H

#include "../kernel/acpi.h"

// Structure data based on https://wiki.osdev.org/HPET

struct address_structure {
	uint8_t address_space_id;
	uint8_t register_bit_width;
	uint8_t register_bit_offset;
	uint8_t reserved;
	uint64_t address;
} __attribute__ ((packed));

struct description_table_header {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oemid[6];
	uint64_t oem_tableid;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__ ((packed));

struct hpet {
	struct acpi_sdt_header header;
	uint8_t hardware_rev_id;
	uint8_t comparator_count:5;
	uint8_t counter_size:1;
	uint8_t reserved:1;
	uint8_t legacy_replacement:1;
	uint16_t pci_vendor_id;
	struct address_structure address;
	uint8_t hpet_number;
	uint16_t minimum_tick;
	uint8_t page_protection;
} __attribute__ ((packed));

void init_hpet();
void start_hpet();
uint32_t get_time();
void init_delay(uint32_t delay);

#endif
