#include <muse/acpi.h>
#include <muse/alloc.h>
#include <muse/apic.h>
#include <muse/logging.h>
#include <muse/paging.h>
#include <muse/trampoline.h>

__attribute__((nonstring)) char rsdp_signature[8] = "RSD PTR ";

void *rsdt_global;

struct rsdp *find_rsdp(struct multiboot_tag_old_acpi *tag_acpi) {
	struct rsdp *res = (struct rsdp *)&tag_acpi->rsdp;
	if (verify_rsdp(res)) {
		log(LOG_INFO, LOG_ACPI, "Found RSDP at %x.\n", res);
		return (struct rsdp *)res;
	}
	return 0;
}

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

void *find_rsdt(struct rsdp *rsdp) {
	if (rsdp->revision == 0) {
		// v1.0
		return (struct rsdt *)(uintptr_t)(rsdp->rsdt_address);
	} else {
		// v2.0+
		return (struct xsdt *)(uintptr_t)(rsdp->xsdt_address);
	}
}

void *find_sdt(char signature[4]) {
	struct rsdt *rsdt_struct = rsdt_global;
	uint32_t entries =
	    (rsdt_struct->header.length - sizeof(struct acpi_sdt_header)) /
	    sizeof(uint32_t);
	for (uint32_t i = 0; i < entries; i++) {
		struct acpi_sdt_header *header = (struct acpi_sdt_header *)((
		    uintptr_t)(rsdt_struct->sdt_ptrs[i]));
		bool correct_signature         = true;
		for (uint32_t j = 0; j < 4; j++) {
			if (header->signature[j] != signature[j]) {
				correct_signature = false;
			}
		}
		if (correct_signature) {
			return header;
		}
	}
	return 0;
}

bool verify_sdt(void *sdt) {
	uint8_t sum     = 0;
	uint32_t length = ((struct acpi_sdt_header *)sdt)->length;
	for (uint32_t i = 0; i < length; i++) {
		sum += ((char *)sdt)[i];
	}
	return sum == 0;
}

extern struct trampoline_info *t_info;

void reinit_acpi() {
	rsdt_global = map_phys_obj(t_info->acpi_rsdt, t_info->rsdt_len);
	struct rsdt *rsdt_struct = rsdt_global;
	uint32_t entries =
	    (rsdt_struct->header.length - sizeof(struct acpi_sdt_header)) /
	    sizeof(uint32_t);
	for (uint32_t i = 0; i < entries; i++) {
		struct acpi_sdt_header *header = map_phys_obj(
		    (void *)(paddr_t)rsdt_struct->sdt_ptrs[i], sizeof(*header));
		size_t len = header->length;
		for (vaddr_t v = ALIGN_PG_DOWN(header);
		     v < ALIGN_PG_UP((void *)header + sizeof(*header));
		     v += PAGE_SIZE) {
			unmap_page(v);
			kfree((void *)v);
		}
		rsdt_struct->sdt_ptrs[i] = (uintptr_t)map_phys_obj(
		    (void *)(paddr_t)rsdt_struct->sdt_ptrs[i], len);
	}
}
