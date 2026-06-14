#include <muse/context.h>
#include <muse/elf.h>
#include <muse/elf_kernel.h>
#include <muse/scheduler.h>
#include <muse/trampoline.h>

extern struct trampoline_info *t_info;

void load_elf_kernel(char *path) {
	struct elf_info elf = parse_elf(path);
	if (!elf.present) {
		return;
	}
	load_elf_data(elf);
	t_info->kernel_limit = (void *)elf.limit;
	create_kernel_context((func_ptr_t)(uintptr_t)elf.header->entry_point,
	                      elf.first_scr);
}
