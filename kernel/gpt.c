#include "gpt.h"
#include "alloc.h"
#include "../drivers/text.h"

void init_gpt(struct ata_dev dev) {
	struct gpt_table_header *header = kmalloc(SECTOR_SIZE);
	ata_transfer(dev, 1, 1, (uint16_t*)header, ATA_READ);
	kprint("GPT table header detected. Table contains ");
	kprint_int(header->partition_count, 10);
	kprint(" partitions.\n");
	struct gpt_partition *partitions = kmalloc(SECTOR_SIZE);
	ata_transfer(dev, 2, 1, (uint16_t*)partitions, ATA_READ);
	for (uint32_t i = 0; i < header->partition_count; i++) {
		uint8_t guid_or = 0; // Will be nonzero if any part of the GUID is nonzero
		for (uint32_t j = 0; j < 16; j++) {
			guid_or |= partitions[i].partition_guid[j];
		}
		if (!guid_or) {
			continue;
		}
		kprint("Detected partition from LBA 0x");
		kprint_int(partitions[i].start_lba, 16);
		kprint(" to 0x");
		kprint_int(partitions[i].end_lba, 16);
		kprint(".\n");
	}
}
