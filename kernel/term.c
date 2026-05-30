#include "term.h"
#include "../drivers/text.h"

uint32_t term_transfer(__attribute__((unused)) struct vfs_inode *inode,
                       __attribute__((unused)) uint32_t offset, uint32_t size,
                       void *data, enum hal_drive_dir dir) {
	if (dir == DRV_WRITE) {
		for (uint32_t i = 0; i < size; i++) {
			kput_char(((char *)data)[i]);
		}
	} else {
		// TODO: stdin
	}
	return size;
}

struct vfs_inode *create_term() {
	struct vfs_inode inode;
	inode.size         = 0;
	inode.present      = true;
	inode.first_child  = 0;
	inode.backend_data = 0;
	inode.transfer     = term_transfer;
	struct vfs_mount_point *mnt =
	    mount(inode, "/dev/term"); /* TODO: Dynamically generate terminal
	                                  mount points. */
	return &mnt->inode;
}
