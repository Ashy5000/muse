#ifndef GPT_H
#define GPT_H

#include <muse/ata.h>
#include <stdbool.h>
#include <stdint.h>

struct gpt_table_header {
	char signature[8];
	uint32_t revision;
	uint32_t header_size;
	uint32_t header_checksum;
	uint32_t rsvd;
	uint64_t header_lba;
	uint64_t alternate_header_lba;
	uint64_t first_usable_block;
	uint64_t last_usable_block;
	char guid[16];
	uint64_t entry_table_lba;
	uint32_t partition_count;
	uint32_t entry_size;
	uint32_t table_checksum;
};

/* The gpt_partition struct holds the raw partition structures that are stored
 * in the partition table. */
struct gpt_partition {
	char partition_type_guid[16];
	char partition_guid[16];
	uint64_t start_lba;
	uint64_t end_lba;
	uint64_t attrs;
	uint16_t name[36]; // UNICODE16-LE encoded
};

/* The partition struct is a higher-level abstracted interface that most of the
 * rest of the kernel should use. */
struct partition {
	struct hal_drive *dev;
	uint32_t start;
	uint32_t limit;
};

bool init_gpt(struct hal_drive *dev);

#endif
