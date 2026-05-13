#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stdbool.h>
#include "ata.h"
#include "gpt.h"

struct ext2_superblock {
	uint32_t total_inodes;
	uint32_t total_blocks;
	uint32_t superuser_blocks;
	uint32_t unallocated_blocks;
	uint32_t unallocated_inodes;
	uint32_t superblock_block;
	uint32_t block_size_log;
	uint32_t frag_size_log;
	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;
	uint32_t last_mount_time;
	uint32_t last_write_time;
	uint16_t mounts_since_consistent;
	uint16_t max_mounts_until_consistent;
	uint16_t signature; // 0xef53
	uint16_t state;
	uint16_t err_handling;
	uint16_t version_min;
	uint32_t last_consistent_time;
	uint32_t forced_consistency_interval;
	uint32_t osid;
	uint32_t version_maj;
	uint16_t reserved_userid;
	uint16_t reserved_groupid;
	// Extended fields:
	uint32_t first_nonreserved_inode;
	uint16_t inode_size;
	uint16_t superblock_block_group;
	uint32_t feats_optional;
	uint32_t feats_required;
	uint32_t feats_readonly;
	char fsid[16];
	char name[16];
	char last_mount_path[64];
	uint32_t compression;
	uint8_t prealloced_file_blocks;
	uint8_t prealloced_dir_blocks;
	uint16_t rsvd;
	char journal_id[16];
	void *journal_inode;
	void *journal_dev;
	void *orphan_inode_head;
};

struct ext2_block_group_descriptor {
	// Block addresses:
	uint32_t block_bitmap_addr;
	uint32_t inode_bitmap_addr;
	uint32_t inode_table;
	// Numbers of things:
	uint16_t unallocated_blocks;
	uint16_t unallocated_inodes;
	uint16_t directory_count;
	char rsvd[14];
};

struct ext2_inode {
	uint16_t mode;
	uint16_t user_id;
	uint32_t size_lo;
	uint32_t access_time;
	uint32_t creation_time;
	uint32_t modification_time;
	uint32_t deletion_time;
	uint16_t group_id;
	uint16_t hard_links;
	uint32_t sector_count;
	uint32_t flags;
	uint32_t os_specific_0;
	uint32_t direct_blocks[12];
	uint32_t indirect_block_single;
	uint32_t indirect_block_double;
	uint32_t indirect_block_triple;
	uint32_t generation;
	uint32_t extended_attrs;
	uint32_t size_hi;
	uint32_t frag_addr;
	char os_specific_1[12];
};

struct ext2_directory_entry {
	uint32_t inode;
	uint16_t len;
	uint8_t name_len;
	uint8_t file_type;
	char name[255]; // Not actually this long, 255 is max.
};

struct ext2_vfs_payload {
	struct ext2_inode inode;
	struct ata_dev *dev;
	struct gpt_partition *partition;
	struct ext2_superblock *superblock;
};

bool detect_ext2(struct ata_dev *dev, struct gpt_partition partition);

#endif
