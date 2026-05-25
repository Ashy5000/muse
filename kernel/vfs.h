#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>

struct vfs_inode;
struct vfs_tnode;

typedef uint32_t (*fn_file_io_t)(struct vfs_inode *, uint32_t, uint32_t,
                                 void *);
typedef void (*fn_truncate_t)(struct vfs_inode *);
typedef void (*fn_register_inode_t)(struct vfs_inode *, struct vfs_tnode *);
typedef struct vfs_inode *(*fn_create_child_t)(struct vfs_inode *, char *);

struct vfs_inode {
	bool present;
	uint32_t refs;

	uint32_t size;

	void *backend_data;

	fn_file_io_t read;
	fn_file_io_t write;
	fn_truncate_t truncate;
	fn_create_child_t create_child_file;

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

#define MODE_READ     1
#define MODE_WRITE    2
#define MODE_APPEND   4
#define MODE_TRUNCATE 8
#define MODE_CREATE   16

struct file {
	struct vfs_inode *inode;
	uint32_t pos;
	uint8_t mode;
};

void mount(struct vfs_inode inode, char *path);
struct vfs_inode *vfs_open(char *path);

#endif
