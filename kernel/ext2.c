#include "ext2.h"
#include "alloc.h"
#include "../drivers/text.h"
#include "vfs.h"

#define EXT2_DIR 0x4000

struct ext2_block_group_descriptor get_block_group_descriptor(struct ext2_superblock *superblock, struct ata_dev *dev, struct gpt_partition *partition, uint32_t idx) {
	uint32_t block_groups = (superblock->total_inodes + superblock->inodes_per_group - 1) / superblock->inodes_per_group;
	uint32_t block_size = 1024 << superblock->block_size_log;
	uint32_t lba = partition->start_lba + (idx * sizeof(struct ext2_block_group_descriptor) / SECTOR_SIZE);
	if (block_size == 1024) {
		lba += block_size * 2 / SECTOR_SIZE;
	} else {
		lba += block_size / SECTOR_SIZE;
	}
	struct ext2_block_group_descriptor *descriptors = kmalloc(SECTOR_SIZE);
	ata_transfer(dev, lba, 1, (uint16_t*)descriptors, ATA_READ);
	struct ext2_block_group_descriptor descriptor = descriptors[idx % (SECTOR_SIZE / sizeof(struct ext2_block_group_descriptor))];
	kfree(descriptors);
	return descriptor;
}

struct ext2_inode get_inode(struct ext2_superblock *superblock, struct ata_dev *dev, struct gpt_partition *partition, uint32_t inode) {
	uint32_t group = (inode - 1) / superblock->inodes_per_group;
	uint32_t idx = (inode - 1) % superblock->inodes_per_group;
	uint32_t inode_size = 128;
	if (superblock->version_maj >= 1) {
		inode_size = superblock->inode_size;
	}
	uint32_t lba = partition->start_lba + (idx * inode_size / SECTOR_SIZE);
	uint32_t block_size = 1024 << superblock->block_size_log;
	struct ext2_block_group_descriptor group_descriptor = get_block_group_descriptor(superblock, dev, partition, group);
	lba += group_descriptor.inode_table * block_size / SECTOR_SIZE;
	struct ext2_inode *inodes = kmalloc(SECTOR_SIZE);
	enum ata_res err = ata_transfer(dev, lba, 1, (uint16_t*)inodes, ATA_READ);
	uint32_t offset = idx % (SECTOR_SIZE / inode_size);
	struct ext2_inode res = *((struct ext2_inode*)(((void*)inodes) + (offset * inode_size)));
	kfree(inodes);
	return res;
}

void get_block_from_inode(uint32_t block_size, struct ata_dev *dev, struct gpt_partition *partition, struct ext2_inode inode, uint32_t n, void *data) {
	if (n < 12) {
		uint32_t lba = partition->start_lba + (inode.direct_blocks[n] * block_size / SECTOR_SIZE);
		ata_transfer(dev, lba, block_size / SECTOR_SIZE, data, ATA_READ);
		return;
	}
	// TODO: Handle singly, doubly, and triply indirect blocks
}

void enumerate_children(struct vfs_inode *inode_v) {
	struct ext2_vfs_payload *payload = inode_v->backend_data;
	struct ext2_inode inode = payload->inode;
	if ((inode.mode & EXT2_DIR) == 0) {
		// Not a directory
		inode_v->first_child = 0;
		return;
	}
	struct ext2_superblock *superblock = payload->superblock;
	uint32_t block_size = 1024 << superblock->block_size_log;
	uint32_t blocks = inode.sector_count * SECTOR_SIZE / block_size;
	void *buf = kmalloc(block_size);
	struct ext2_directory_entry *entry = buf;
	for (uint32_t i = 0; i < blocks; i++) {
		uint32_t lba = payload->partition->start_lba + (inode.direct_blocks[i] * block_size / SECTOR_SIZE);
		ata_transfer(payload->dev, lba, block_size / SECTOR_SIZE, buf, ATA_READ);
		while((void*)entry - buf < block_size - 1) {
			if (entry->inode == 0) {
				continue;
			}
			struct vfs_tnode *tnode = kmalloc(sizeof(struct vfs_tnode));
			char *name = kmalloc(entry->name_len);
			kmemcpy(name, entry->name, entry->name_len);
			tnode->name = name;
			tnode->name_len = entry->name_len;
			tnode->next = 0;
			tnode->inode.present = false;
			tnode->inode_id = entry->inode;
			tnode->next = inode_v->first_child;
			inode_v->first_child = tnode;
			entry = ((void*)entry) + entry->len;
		}
		entry = (struct ext2_directory_entry*)(buf + (((uintptr_t)entry) % block_size));
	}
	kfree(buf);
}

void ext2_register_inode(struct vfs_inode *parent, struct vfs_tnode *tchild) {
	struct vfs_inode child;
	struct ext2_vfs_payload *child_payload = kmalloc(sizeof(struct ext2_vfs_payload));
	child.backend_data = child_payload;
	struct ext2_vfs_payload *parent_payload = parent->backend_data;
	child_payload->superblock = parent_payload->superblock;
	child_payload->dev = parent_payload->dev;
	child_payload->partition = parent_payload->partition;
	child_payload->inode = get_inode(child_payload->superblock, child_payload->dev, child_payload->partition, tchild->inode_id);
	enumerate_children(&child);
	child.present = true;
	child.register_inode = ext2_register_inode;
	tchild->inode = child;
}

bool detect_ext2(struct ata_dev *dev, struct gpt_partition *partition) {
	struct ext2_superblock *superblock = kmalloc(SECTOR_SIZE * 2);
	ata_transfer(dev, partition->start_lba + 2, 2, (uint16_t*)superblock, ATA_READ);
	if (superblock->signature != 0xef53) {
		return false;
	}

	kprint("Found EXT2 superblock with version ");
	kprint_int(superblock->version_maj, 10);
	kprint(".");
	kprint_int(superblock->version_min, 10);
	kprint(". inode size: 0x");
	kprint_int(superblock->inode_size, 16);
	kprint(".\n");
	if (superblock->version_maj >= 1) {
		kprint("Required feature flags: 0x");
		kprint_int(superblock->feats_required, 16);
		kprint(". Read-only feature flags: 0x");
		kprint_int(superblock->feats_readonly, 16);
		kprint(".\n");
	}

	struct ext2_inode root = get_inode(superblock, dev, partition, 2);
	struct vfs_inode inode;
	struct ext2_vfs_payload *payload = kmalloc(sizeof(struct ext2_vfs_payload));
	payload->inode = root;
	payload->dev = dev;
	payload->partition = partition;
	payload->superblock = superblock;
	inode.backend_data = payload;
	inode.first_child = 0;
	inode.register_inode = ext2_register_inode;
	enumerate_children(&inode);
	mount(inode, "/ext2");
	// inode.register_inode(&inode, inode.first_child);
	// struct vfs_inode dir = inode.first_child->inode;
	struct vfs_inode test_file = open("/ext2/testdir/test.txt");
	if (test_file.present) {
		kprint("Opened file!\n");
	}

	kfree(superblock);

	return true;
}
