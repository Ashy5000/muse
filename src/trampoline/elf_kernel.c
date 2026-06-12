#include <muse/context.h>
#include <muse/elf.h>
#include <muse/elf_kernel.h>
#include <muse/scheduler.h>

void load_elf_kernel(char *path) {
	struct elf_info elf = parse_elf(path);
	if (!elf.present) {
		return;
	}
	load_elf_data(elf);
	create_kernel_context((func_ptr_t)(uintptr_t)elf.header->entry_point,
	                      elf.first_scr);
}
