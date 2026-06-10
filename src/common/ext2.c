#include <muse/ext2.h>
#include <muse/alloc.h>
#include <muse/logging.h>
#include <muse/utils.h>
#include <muse/vfs.h>
#include <stddef.h>

#define EXT2_DIR 0x4000

__attribute__((warn_unused_result)) enum hal_drive_res
group_descriptor_transfer(struct ext2_superblock *superblock,
                          struct vfs_inode *bdev, uint32_t idx,
                          struct ext2_block_group_descriptor *descriptor,
                          enum data_dir dir) {
	uint32_t block_size = 1024 << superblock->block_size_log;
	uint32_t lba =
	    (idx * sizeof(struct ext2_block_group_descriptor) / SECTOR_SIZE);
	lba += MAX(block_size, 2048) / SECTOR_SIZE;
	struct ext2_block_group_descriptor *descriptors = kmalloc(SECTOR_SIZE);
	enum hal_drive_res err =
	    bdev->transfer(bdev, lba, 1, descriptors, DIR_READ);
	if (err) {
		return err;
	}
	if (dir == DIR_WRITE) {
		descriptors[idx %
		            (SECTOR_SIZE /
		             sizeof(struct ext2_block_group_descriptor))] =
		    *descriptor;
		err = bdev->transfer(bdev, lba, 1, descriptors, DIR_READ);
		if (err) {
			return err;
		}
	} else {
		*descriptor =
		    descriptors[idx %
		                (SECTOR_SIZE /
		                 sizeof(struct ext2_block_group_descriptor))];
	}
	kfree(descriptors);
	return DRV_SUCCESS;
}

enum hal_drive_res inode_transfer(struct ext2_superblock *superblock,
                                  struct vfs_inode *bdev, uint32_t inode_i,
                                  struct ext2_inode *inode, enum data_dir dir) {
	uint32_t group      = (inode_i - 1) / superblock->inodes_per_group;
	uint32_t idx        = (inode_i - 1) % superblock->inodes_per_group;
	uint32_t inode_size = 128;
	if (superblock->version_maj >= 1) {
		inode_size = superblock->inode_size;
	}
	uint32_t lba        = (idx * inode_size / SECTOR_SIZE);
	uint32_t block_size = 1024 << superblock->block_size_log;

	struct ext2_block_group_descriptor group_descriptor;
	enum hal_drive_res err = group_descriptor_transfer(
	    superblock, bdev, group, &group_descriptor, DIR_READ);
	if (err) {
		return err;
	}

	lba += group_descriptor.inode_table * block_size / SECTOR_SIZE;
	struct ext2_inode *inodes = kmalloc(SECTOR_SIZE);
	err = bdev->transfer(bdev, lba, 1, inodes, DIR_READ);
	if (err) {
		kfree(inodes);
		return err;
	}

	uint32_t offset = idx % (SECTOR_SIZE / inode_size);
	log(LOG_DEBUG, LOG_EXT2, "inode offset: %x.\n",
	    lba * SECTOR_SIZE + offset * inode_size);

	if (dir == DIR_READ) {
		*inode = *((struct ext2_inode *)(((void *)inodes) +
		                                 (offset * inode_size)));
	} else {
		*((struct ext2_inode *)(((void *)inodes) +
		                        (offset * inode_size))) = *inode;
		err = bdev->transfer(bdev, lba, 1, inodes, DIR_WRITE);
		if (err) {
			kfree(inodes);
			return err;
		}
	}
	kfree(inodes);
	return DRV_SUCCESS;
}

/* Attempts to allocate either a data block or an inode for a particular block
 * group.
 * Data block (inode = false): Returns the block address of the allocated
 *	block, or 0 if no free block is found.
 * Inode (inode = true): Returns the
 *	inode's ID, which can be passed to get_inode() to retrieve the inode
 *	itself. Returns 0 if no free inode is found.
 */
uint32_t alloc_ext2_obj(struct ext2_superblock *superblock,
                        struct vfs_inode *bdev, uint32_t group, bool inode) {
	struct ext2_block_group_descriptor descriptor;
	enum hal_drive_res err = group_descriptor_transfer(
	    superblock, bdev, group, &descriptor, DIR_READ);
	if (err) {
		return 0;
	}
	if (inode && !descriptor.unallocated_inodes) {
		return 0;
	}
	if (!inode && !descriptor.unallocated_blocks) {
		return 0;
	}
	uint32_t block_size = 1024 << superblock->block_size_log;
	err = group_descriptor_transfer(superblock, bdev, group, &descriptor,
	                                DIR_READ);
	if (err) {
		return 0;
	}
	uint32_t bitmap_lba;
	if (inode) {
		bitmap_lba =
		    (descriptor.inode_bitmap_addr * block_size / SECTOR_SIZE);
	} else {
		bitmap_lba =
		    (descriptor.block_bitmap_addr * block_size / SECTOR_SIZE);
	}
	uint8_t *bitmap = kmalloc(block_size);
	err = bdev->transfer(bdev, bitmap_lba, block_size / SECTOR_SIZE, bitmap,
	                     DIR_READ);
	if (err) {
		return 0;
	}
	for (size_t i = 0; i < block_size; i++) {
		uint8_t bitmap_byte = bitmap[i];
		if (bitmap_byte == 0xFF) {
			continue;
		}
		for (uint8_t j = 0; j < 8; j++) {
			if (!((bitmap_byte >> j) & 1)) {
				bitmap[i] |= 1 << j;
				err = bdev->transfer(bdev, bitmap_lba,
				                     block_size / SECTOR_SIZE,
				                     bitmap, DIR_WRITE);
				kfree(bitmap);
				if (err) {
					return 0;
				}
				log(LOG_DEBUG, LOG_EXT2,
				    "alloc_ext2_obj(): found available "
				    "object at block %x. i = %i, j = %i.\n",
				    (group * superblock->blocks_per_group) +
				        (i * 8) + j,
				    i, j);
				/* FIXME: Update free block/inode count in block
				 * descriptor */
				if (inode) {
					return (group *
					        superblock->inodes_per_group) +
					       (i * 8) + j + 1;
				} else {
					return (group *
					        superblock->blocks_per_group) +
					       (i * 8) + j;
				}
			}
		}
	}
	kfree(bitmap);
	THERE_ARE_FOUR_LIGHTS("a EXT2 block group's bitmap did not match its "
	                      "unallocated object count");
	return 0;
}

enum hal_drive_res inode_data_transfer(struct ext2_superblock *superblock,
                                       struct vfs_inode *bdev, uint32_t inode_i,
                                       struct ext2_inode *inode, uint32_t idx,
                                       void *data, enum data_dir dir) {
	enum hal_drive_res err = 0;
	uint32_t block_size    = 1024 << superblock->block_size_log;
	uint32_t lba           = DRV_ERR_BAD_ARGS;
	bool write_inode       = false;
	if (idx < 12) {
		if (!inode->direct_blocks[idx]) {
			log(LOG_DEBUG, LOG_EXT2, "inode_i: %x.\n", inode_i);
			inode->direct_blocks[idx] = alloc_ext2_obj(
			    superblock, bdev, inode->group_id, false);
			write_inode = true;
			if (err) {
				log(LOG_WARN, LOG_EXT2,
				    "inode_transfer returned error code %i.\n",
				    err);
				return err;
			}
		}
		lba = inode->direct_blocks[idx] * block_size / SECTOR_SIZE;
	} else if (idx < 12 + (block_size / sizeof(uint32_t))) {
		if (!inode->indirect_block_single) {
			if (dir == DIR_READ) {
				return DRV_ERR_BAD_ARGS;
			}
			inode->indirect_block_single = alloc_ext2_obj(
			    superblock, bdev, inode->group_id, false);
			write_inode = true;
		}
		// Singly indirect block
		uint32_t *indirect_block = kmalloc(block_size);
		err = bdev->transfer(bdev, inode->indirect_block_single,
		                     block_size / SECTOR_SIZE, indirect_block,
		                     DIR_READ);
		if (err) {
			return err;
		}
		lba = indirect_block[idx - 12] * block_size / SECTOR_SIZE;
		if (!lba) {
			indirect_block[idx - 12] = alloc_ext2_obj(
			    superblock, bdev, inode->group_id, false);
			err = bdev->transfer(bdev, inode->indirect_block_single,
			                     block_size / SECTOR_SIZE,
			                     indirect_block, DIR_WRITE);
			lba =
			    indirect_block[idx - 12] * block_size / SECTOR_SIZE;
		}
	}

	if (dir == DIR_WRITE) {
		log(LOG_DEBUG, LOG_EXT2, "Transferring to LBA %x.\n", lba);
	}
	// TODO: Handle doubly and triply indirect blocks
	err = bdev->transfer(bdev, lba, block_size / SECTOR_SIZE, data, dir);
	if (err) {
		return err;
	}

	if (write_inode) {
		err =
		    inode_transfer(superblock, bdev, inode_i, inode, DIR_WRITE);
	}
	return err;
}

enum hal_drive_res enumerate_children(struct vfs_inode *inode_v) {
	struct ext2_vfs_payload *payload = inode_v->backend_data;
	struct ext2_inode inode          = payload->inode;
	if ((inode.mode & EXT2_DIR) == 0) {
		// Not a directory
		inode_v->first_child = 0;
		return DRV_SUCCESS;
	}
	struct ext2_superblock *superblock = payload->superblock;
	uint32_t block_size                = 1024 << superblock->block_size_log;
	uint32_t blocks = inode.sector_count * SECTOR_SIZE / block_size;
	void *buf       = kmalloc(block_size * blocks);
	struct ext2_directory_entry *entry = buf;
	enum hal_drive_res err;
	for (uint32_t i = 0; i < blocks; i++) {
		err = inode_data_transfer(payload->superblock, payload->bdev,
		                          payload->inode_id, &inode, i,
		                          buf + (block_size * i), DIR_READ);
		if (err) {
			return err;
		}
	}
	while ((uintptr_t)entry - (uintptr_t)buf < block_size * blocks - 1) {
		if (entry->inode == 0) {
			break;
		}
		struct vfs_tnode *tnode = kmalloc(sizeof(*tnode));
		char *name              = kmalloc(entry->name_len);
		memcpy(name, entry->name, entry->name_len);
		tnode->name          = name;
		tnode->name_len      = entry->name_len;
		tnode->next          = 0;
		tnode->inode.present = false;
		tnode->inode_id      = entry->inode;
		tnode->next          = inode_v->first_child;
		inode_v->first_child = tnode;
		entry                = ((void *)entry) + entry->len;
	}
	kfree(buf);
	return DRV_SUCCESS;
}

uint32_t alloc_child_inode(struct vfs_inode *parent) {
	struct ext2_vfs_payload *payload = parent->backend_data;
	uint32_t parent_group            = payload->group;
	uint32_t group_cnt = payload->superblock->total_blocks /
	                     payload->superblock->blocks_per_group;
	/* Check every block group, moving outward from the parent group. This
	 * will locate the allocated inode as close to the parent as possible.
	 */
	for (uint32_t d = 0;
	     d < MAX(parent_group, group_cnt - parent_group - 1); d++) {
		uint32_t group;
		if (parent_group >= d) {
			// Check earlier inode
			group          = parent_group - d;
			uint32_t inode = alloc_ext2_obj(
			    payload->superblock, payload->bdev, group, true);
			if (inode) {
				return inode;
			}
		}
		if (parent_group + d < group_cnt) {
			// Check later inode
			group          = parent_group + d;
			uint32_t inode = alloc_ext2_obj(
			    payload->superblock, payload->bdev, group, true);
			if (inode) {
				return inode;
			}
		}
	}
	return 0;
}

// struct vfs_inode *ext2_create_child_file(struct vfs_inode *parent,
//                                          char *child_name) {
// 	struct vfs_tnode *child_t;
// 	child_t->name     = child_name;
// 	child_t->name_len = 0;
// 	while (child_name[child_t->name_len]) {
// 		child_t->name_len++;
// 	}
// 	child_t->next       = parent->first_child;
// 	parent->first_child = child_t;
// 	// Allocate an inode, try to stay in same block group as parent
// 	uint32_t inode_id   = alloc_child_inode(parent);
// 	if (!inode_id) {
// 		return 0;
// 	}
//
// 	struct ext2_vfs_payload *payload = parent->backend_data;
// 	uint32_t block_size = 1024 << payload->superblock->block_size_log;
// 	uint32_t blocks     = payload->inode.size_lo / block_size;
// void *buf           = kmalloc(block_size * blocks);
// 	struct ext2_directory_entry *entry = buf;
// 	for (uint32_t i = 0; i < blocks; i++) {
// 		if (inode_data_transfer(block_size, payload->dev,
// 		                        payload->partition, payload->inode, i,
// 		                        buf + (block_size * i), DRV_READ)) {
// 			return 0;
// 		}
// 	}
// 	while ((uintptr_t)entry - (uintptr_t)buf < block_size * blocks - 1) {
// 		if (entry->inode == 0) {
// 			break;
// 		}
// 		entry = ((void *)entry) + entry->len;
// 	}
// 	size_t entry_size  = sizeof(struct ext2_directory_entry) -
// 	                     ((255 - child_t->name_len) * sizeof(char));
// 	uint32_t block_idx = ((void *)entry - buf + entry_size) / block_size;
// 	kfree(buf);
// }

uint32_t ext2_transfer(struct vfs_inode *inode, uint32_t offset, uint32_t len,
                       void *data, enum data_dir dir) {
	struct ext2_vfs_payload *payload = inode->backend_data;
	uint32_t block_size = 1024 << payload->superblock->block_size_log;

	if (offset + len > payload->inode.size_lo) {
		len = payload->inode.size_lo - offset;
	}

	void *bfr = kmalloc(block_size);
	uint32_t block =
	    offset /
	    block_size; /* The index of the block we are currently reading. */
	uint32_t block_start =
	    block * block_size; /* Where the current block starts. */
	uint32_t block_end =
	    (block + 1) * block_size; /* Where the current block ends. */
	uint32_t bytes_written = 0;
	enum hal_drive_res err;
	for (;;) {
		err = inode_data_transfer(payload->superblock, payload->bdev,
		                          payload->inode_id, &payload->inode,
		                          block, bfr, DIR_READ);
		if (err) {
			break;
		}
		uint32_t skip = 0; /* The number of bytes from the beginning of
		                      the block that we should skip */
		if (bytes_written == 0) {
			skip = offset - block_start;
		}
		uint32_t cnt = block_size;
		if (offset + len < block_end) {
			cnt += offset + len - block_end;
		}
		cnt -= skip;
		if (dir == DIR_READ) {
			memcpy(data + bytes_written, bfr + skip, cnt);
		} else {
			memcpy(bfr + skip, data + bytes_written, cnt);
			err = inode_data_transfer(
			    payload->superblock, payload->bdev,
			    payload->inode_id, &payload->inode, block, bfr,
			    DIR_WRITE);
			if (err) {
				break;
			}
		}
		bytes_written += cnt;
		if (bytes_written == len) {
			break;
		}
		block++;
		block_start += block_size;
		block_end += block_size;
	}

	if (dir == DIR_WRITE && offset + len >= payload->inode.size_lo) {
		payload->inode.size_lo = offset + len;
		payload->inode.sector_count =
		    (payload->inode.size_lo + SECTOR_SIZE - 1) / SECTOR_SIZE;
		err = inode_transfer(payload->superblock, payload->bdev,
		                     payload->inode_id, &payload->inode,
		                     DIR_WRITE);
	}

	kfree(bfr);
	return bytes_written;
}

void ext2_register_inode(struct vfs_inode *parent, struct vfs_tnode *tchild) {
	struct vfs_inode child;
	struct ext2_vfs_payload *child_payload =
	    kmalloc(sizeof(struct ext2_vfs_payload));
	child.backend_data                      = child_payload;
	struct ext2_vfs_payload *parent_payload = parent->backend_data;
	child_payload->superblock               = parent_payload->superblock;
	child_payload->bdev                     = parent_payload->bdev;
	child_payload->inode_id                 = tchild->inode_id;
	inode_transfer(child_payload->superblock, child_payload->bdev,
	               tchild->inode_id, &child_payload->inode, DIR_READ);
	child_payload->group = (tchild->inode_id - 1) /
	                       child_payload->superblock->inodes_per_group;
	enumerate_children(&child);
	child.present        = true;
	child.register_inode = ext2_register_inode;
	child.transfer       = ext2_transfer;
	child.size           = child_payload->inode.size_lo;
	tchild->inode        = child;
}

enum hal_drive_res detect_ext2(struct vfs_inode *bdev) {
	struct ext2_superblock *superblock = kmalloc(SECTOR_SIZE * 2);

	enum hal_drive_res err =
	    bdev->transfer(bdev, 2, 2, superblock, DIR_READ);
	if (err) {
		log(LOG_ERROR, LOG_EXT2, "Failed to read superblock!\n");
		kfree(superblock);
		return err;
	}

	if (superblock->signature != 0xef53) {
		log(LOG_DEBUG, LOG_EXT2,
		    "Partition is not an EXT2 filesystem.\n");
		kfree(superblock);
		return DRV_SUCCESS;
	}

	// TODO: Handle required feature flags
	log(LOG_INFO, LOG_EXT2, "Required feature flags: %x.\n",
	    superblock->feats_required);

	log(LOG_DEBUG, LOG_EXT2, "EXT2 superblock starting block: %i.\n",
	    superblock->superblock_block);

	struct ext2_inode root;
	inode_transfer(superblock, bdev, 2, &root, DIR_READ);

	struct vfs_inode inode;
	struct ext2_vfs_payload *payload =
	    kmalloc(sizeof(struct ext2_vfs_payload));
	payload->inode       = root;
	payload->bdev        = bdev;
	payload->superblock  = superblock;
	payload->group       = 0;
	inode.backend_data   = payload;
	inode.first_child    = 0;
	inode.register_inode = ext2_register_inode;
	inode.transfer       = ext2_transfer;
	enumerate_children(&inode);
	mount(inode, "/ext2");

	return true;
}
