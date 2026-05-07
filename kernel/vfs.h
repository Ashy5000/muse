#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>

struct vfs_inode;
struct vfs_tnode;

typedef void (*fn_read_t)(struct vfs_inode*, uint32_t, uint32_t, void*);
typedef void (*fn_write_t)(struct vfs_inode*, uint32_t, uint32_t, void*);
typedef void (*fn_register_inode_t)(struct vfs_inode*, struct vfs_tnode*);

struct vfs_inode {
	bool present;
	uint32_t refs;

	void *backend_data;

	fn_read_t read;
	fn_write_t write;

	// Directory + mount point only
	struct vfs_tnode *first_child;
	fn_register_inode_t register_inode;
};

struct vfs_tnode {
	char *name;
	uint32_t name_len;
	struct vfs_inode inode;
	struct vfs_tnode *next;
	uint32_t inode_id; // There's probably a better way to do this
};

struct vfs_mount_point {
	struct vfs_inode inode;
	char *path;
	struct vfs_mount_point *next;
};

void mount(struct vfs_inode inode, char *path);
struct vfs_inode vfs_open(char *path);

#endif
