#include "vfs.h"
#include "alloc.h"
#include "../drivers/text.h"

struct vfs_mount_point *first_mount_point = 0;

void mount(struct vfs_inode inode, char *path) {
	struct vfs_mount_point *mount_point = kmalloc(sizeof(struct vfs_mount_point));
	mount_point->inode = inode;
	mount_point->path = path;
	mount_point->next = 0;
	if (first_mount_point) {
		mount_point->next = first_mount_point;
	}
	first_mount_point = mount_point;
}

struct vfs_inode open(char *path) {
	struct vfs_mount_point *mount_point = first_mount_point;
	bool success = false;
	char *start = path + 1; // Account for the '/' after mount point path
	while (mount_point) {
		uint32_t i = 0;
		bool match = true;
		while (path[i] && mount_point->path[i]) {
			if (path[i] != mount_point->path[i]) {
				match = false;
				break;
			}
			i++;
		}
		if (mount_point->path[i]) {
			match = false;
		}
		if (match) {
			success = true;
			start += i;
			break;
		}
		mount_point = mount_point->next;
	}
	struct vfs_inode inode;
	inode.present = false;
	if (!success) {
		return inode;
	}
	inode = mount_point->inode;
	char *end = start;
	while (*start) {
		while (*end && (*end != '/')) {
			end++;
		}

		struct vfs_tnode *tnode = inode.first_child;
		bool success = false;
		while (tnode) {
			bool match = true;
			if (tnode->name_len != end - start) {
				match = false;
			}
			for (uint32_t i = 0; i < tnode->name_len; i++) {
				if (tnode->name[i] != start[i]) {
					match = false;
					break;
				}
			}
			if (!match) {
				tnode = tnode->next;
				continue;
			}
			if (!tnode->inode.present) {
				inode.register_inode(&inode, tnode);
			}
			inode = tnode->inode;
			success = true;
			break;
		}
		if (!success) {
			break;
		}
		end++;
		start = end;
	}

	return inode;
}
