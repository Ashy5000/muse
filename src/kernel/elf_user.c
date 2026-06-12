#include <muse/elf.h>
#include <muse/elf_kernel.h>
#include <muse/userspace.h>

void load_elf_user(char *path, uint32_t argc, char **argv,
                   struct vfs_inode *stdin, struct vfs_inode *stdout,
                   struct vfs_inode *stderr) {
	struct elf_info elf = parse_elf(path);
	if (!elf.present) {
		return;
	}
	load_user_call_info((func_ptr_t)(uintptr_t)elf.header->entry_point,
	                    argc, argv);
	create_context(enter_ring3, 1, true, elf.first_scr, elf.limit, stdin,
	               stdout, stderr);
	load_elf_data(elf);
}
