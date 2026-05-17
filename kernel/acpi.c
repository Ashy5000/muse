#include <stdbool.h>
#include "acpi.h"
#include "../drivers/text.h"

__attribute__ ((nonstring)) char rsdp_signature[8] = "RSD PTR ";

struct rsdp *rsdp_global;
void *rsdt_global;

struct rsdp *find_rsdp() {
	for (char *rsdp = (char*)0x000E0000; (uintptr_t)rsdp < 0x000FFFFF; rsdp += 0x10) {
		bool correct_signature = true;
		for (uint32_t i = 0; i < 8; i++) {
			if (rsdp[i] != rsdp_signature[i]) {
				correct_signature = false;
				break;
			}
		}
		if (correct_signature) {
			return (struct rsdp*)rsdp;
		}
	}
	return 0;
};

bool verify_rsdp(struct rsdp *rsdp) {
	uint8_t sum = 0;
	for (uint32_t i = 0; i < 8; i++) {
		sum += rsdp->signature[i];
	}
	sum += rsdp->checksum;
	for (uint32_t i = 0; i < 6; i++) {
		sum += rsdp->oem_id[i];
	}
	sum += rsdp->revision;
	for (uint32_t i = 0; i < 32; i += 8) {
		sum += (rsdp->rsdt_address >> i) & 0xff;
	}
	if (sum != 0) {
		return false;
	}
	if (rsdp->revision == 2) {
		// v2.0+
		uint8_t sum = 0;
		for (uint32_t i = 0; i < 32; i += 8) {
			sum += (rsdp->length >> i) & 0xff;
		}
		for (uint32_t i = 0; i < 64; i += 8) {
			sum += (rsdp->rsdt_address >> i) & 0xff;
		}
		sum += rsdp->extended_checksum;
		for (uint32_t i = 0; i < 3; i++) {
			sum += rsdp->reserved[i];
		}
		return sum == 0;
	}
	return true;
}

void *find_rsdt() {
	if (rsdp_global->revision == 0) {
		// v1.0
		return (struct rsdt*)(uintptr_t)(rsdp_global->rsdt_address);
	} else {
		// v2.0+
		return (struct xsdt*)(uintptr_t)(rsdp_global->xsdt_address);
	}
}

void *find_sdt(char signature[4]) {
	struct rsdt *rsdt_struct = rsdt_global;
	uint32_t entries = (rsdt_struct->header.length - sizeof(struct acpi_sdt_header)) / sizeof(uint32_t);
	for (uint32_t i = 0; i < entries; i++) {
		struct acpi_sdt_header *header = (struct acpi_sdt_header*)((uintptr_t)(rsdt_struct->sdt_ptrs[i]));
		bool correct_signature = true;
		for (uint32_t j = 0; j < 4; j++) {
			if (header->signature[j] != signature[j]) {
				correct_signature = false;
			}
		}
		if (correct_signature) {
			return header;
		}
	}
	kprint("Failed to find SDT!\n");
	return 0;
}

bool verify_sdt(void *sdt) {
	uint8_t sum = 0;
	uint32_t length = ((struct acpi_sdt_header*)sdt)->length;
	for (uint32_t i = 0; i < length; i++) {
		sum += ((char*)sdt)[i];
	}
	return sum == 0;
}

void init_acpi() {
	rsdp_global = find_rsdp();
	rsdt_global = find_rsdt();
}
