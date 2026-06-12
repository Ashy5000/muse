#ifndef ELF_USER_H
#define ELF_USER_H

#include <stdint.h>

void load_elf_user(char *path, uint32_t argc, char **argv,
                   struct vfs_inode *stdin, struct vfs_inode *stdout,
                   struct vfs_inode *stderr);

#endif
