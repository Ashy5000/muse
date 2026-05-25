#include "gpt.h"
#include "alloc.h"
#include "ext2.h"
#include "logging.h"

bool init_gpt(struct hal_drive *dev) {
	struct partition partition_raw;
	partition_raw.dev               = dev;
	partition_raw.start             = 0;
	partition_raw.limit             = -1;
	struct gpt_table_header *header = kmalloc(SECTOR_SIZE);
	if (dev->transfer(&partition_raw, 1, 1, (uint16_t *)header, DRV_READ)) {
		return false;
	}
	log(LOG_INFO, LOG_GPT, "GPT table detected with %i partitions.\n",
	    header->partition_count);
	uint32_t num_sectors             = (header->partition_count + 3) / 4;
	// Go one sector at a time to save memory
	struct gpt_partition *partitions = kmalloc(SECTOR_SIZE);
	for (uint32_t i = 0; i < num_sectors; i++) {
		if (dev->transfer(&partition_raw, 2 + i, 1,
		                  (uint16_t *)partitions, DRV_READ)) {
			return false;
		}
		for (uint32_t j = 0; j < 4; j++) {
			uint8_t guid_nonzero = 0;
			for (uint32_t k = 0; k < 16; k++) {
				guid_nonzero |= partitions[j].partition_guid[k];
			}
			if (!guid_nonzero) {
				continue;
			}
			log(LOG_DEBUG, LOG_GPT,
			    "Detected partition from LBA %x-%x.\n",
			    partitions[j].start_lba, partitions[j].end_lba);
			struct partition *abstract_partition =
			    kmalloc(sizeof(*abstract_partition));
			abstract_partition->dev   = dev;
			abstract_partition->start = partitions[j].start_lba;
			abstract_partition->limit = partitions[j].end_lba;
			detect_ext2(dev, abstract_partition);
		}
	}
	kfree(partitions);
	kfree(header);
	return true;
}
