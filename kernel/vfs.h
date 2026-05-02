#ifndef VFS_H
#define VFS_H

#include <stdint.h>

struct vfs_inode;
struct vfs_tnode;

typedef void (*fn_read_t)(struct vfs_inode*, void*, uint32_t);
typedef void (*fn_write_t)(struct vfs_inode*, void*, uint32_t);
typedef void (*fn_register_inode_t)(struct vfs_tnode*);

struct vfs_inode {
	fn_read_t read;
	fn_write_t write;
	// Directory + mount point only
	struct vfs_tnode *children;
	uint32_t children_count;
};

struct vfs_tnode {
	char *name;
	uint32_t name_len;
	struct vfs_inode *inode;
	fn_register_inode_t register_inode;
};

struct vfs_mount_point {
	struct vfs_tnode *tnode;
	char *path;
	struct vfs_mount_point *next;
};

void mount(struct vfs_tnode *tnode, char *path);

#endif
