#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

struct rsdp {
	char signature[8];
	uint8_t checksum;
	char oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;

	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum;
	uint8_t reserved[3];
} __attribute__ ((packed));

struct rsdp *find_rsdp();
bool verify_rsdp(struct rsdp *rsdp);

struct acpi_sdt_header {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
};

struct rsdt {
	struct acpi_sdt_header header;
	uint32_t sdt_ptrs[];
} __attribute__ ((packed));

struct xsdt {
	struct acpi_sdt_header header;
	uint64_t sdt_ptrs[];
} __attribute__ ((packed));

void *find_rsdt();

void *find_sdt(char signature[4]);
bool verify_sdt(void *sdt);

void init_acpi();

#endif
