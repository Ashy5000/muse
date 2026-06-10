#include "gpt.h"
#include "../drivers/text.h"
#include "alloc.h"
#include "ext2.h"
#include "logging.h"
#include "vfs.h"

/* Transfers data from a mounted GPT partition. Because this is a block device,
 * offset and size are in LBA. (Is there a better place to put this?) */
enum hal_drive_res gpt_transfer(struct vfs_inode *inode, uint32_t offset,
                                uint32_t size, void *data, enum data_dir dir) {
	struct partition *part = inode->backend_data;
	return part->dev->transfer(part, offset, size, data, dir);
}

bool init_gpt(struct hal_drive *dev) {
	struct partition partition_raw;
	partition_raw.dev               = dev;
	partition_raw.start             = 0;
	partition_raw.limit             = -1;
	struct gpt_table_header *header = kmalloc(SECTOR_SIZE);
	if (dev->transfer(&partition_raw, 1, 1, header, DIR_READ)) {
		return false;
	}
	if (memcmp(header->signature, "EFI PART", 8)) {
		return false;
	}
	log(LOG_INFO, LOG_GPT, "GPT table detected with %i entries.\n",
	    header->partition_count);
	uint32_t num_sectors             = (header->partition_count + 3) / 4;
	// Go one sector at a time to save memory
	struct gpt_partition *partitions = kmalloc(SECTOR_SIZE);
	for (uint32_t i = 0; i < num_sectors; i++) {
		if (dev->transfer(&partition_raw, 2 + i, 1,
		                  (uint16_t *)partitions, DIR_READ)) {
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
			log(LOG_INFO, LOG_GPT,
			    "Detected partition from LBA %x-%x.\n",
			    (uint32_t)partitions[j].start_lba,
			    (uint32_t)partitions[j].end_lba);
			struct partition *abstract_partition =
			    kmalloc(sizeof(*abstract_partition));
			abstract_partition->dev   = dev;
			abstract_partition->start = partitions[j].start_lba;
			abstract_partition->limit = partitions[j].end_lba;
			struct vfs_inode *inode   = kmalloc(sizeof(*inode));
			inode->backend_data       = abstract_partition;
			inode->present            = true;
			inode->transfer           = gpt_transfer;
			inode->size               = abstract_partition->limit -
			                            abstract_partition->start;
			inode->type               = DEV_BLOCK;
			/* TODO: Mount this using some fancy naming scheme */
			detect_ext2(inode);
		}
	}
	kfree(partitions);
	kfree(header);
	return true;
}
