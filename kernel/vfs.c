#include "vfs.h"
#include "alloc.h"

struct vfs_mount_point *first_mount_point = 0;

void mount(struct vfs_tnode *tnode, char *path) {
	struct vfs_mount_point *mount_point = kmalloc(sizeof(struct vfs_mount_point));
	mount_point->tnode = tnode;
	mount_point->path = path;
	mount_point->next = 0;
	if (first_mount_point) {
		mount_point->next = first_mount_point;
	}
	first_mount_point = mount_point;
}
