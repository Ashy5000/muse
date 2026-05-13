#include "gpt.h"
#include "alloc.h"
#include "../drivers/text.h"
#include "ext2.h"

void init_gpt(struct ata_dev *dev) {
	struct gpt_table_header *header = kmalloc(SECTOR_SIZE);
	ata_transfer(dev, 1, 1, (uint16_t*)header, ATA_READ);
	kprint("GPT table header detected. Table contains ");
	kprint_int(header->partition_count, 10);
	kprint(" partitions.\n");
	uint32_t num_sectors = (header->partition_count + 3) / 4;
	// Go one sector at a time to save memory
	for (uint32_t i = 0; i < num_sectors; i++) {
		struct gpt_partition *partitions = kmalloc(SECTOR_SIZE);
		ata_transfer(dev, 2 + i, 1, (uint16_t*)partitions, ATA_READ);
		for (uint32_t j = 0; j < 4; j++) {
			uint8_t guid_nonzero = 0;
			for (uint32_t k = 0; k < 16; k++) {
				guid_nonzero |= partitions[j].partition_guid[k];
			}
			if (!guid_nonzero) {
				continue;
			}
			kprint("Detected partition from LBA 0x");
			kprint_int(partitions[j].start_lba, 16);
			kprint("-0x");
			kprint_int(partitions[j].end_lba, 16);
			kprint(".\n");
			detect_ext2(dev, partitions[j]);
		}
		kfree(partitions);
	}
	kfree(header);
}
