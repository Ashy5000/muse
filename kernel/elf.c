#include "elf.h"
#include "vfs.h"
#include "alloc.h"
#include "scroll.h"
#include "context.h"
#include "userspace.h"
#include "../drivers/text.h"

#define PT_LOAD 1

void load_elf(char *path, uint32_t argc, char **argv) {
	struct vfs_inode file = vfs_open(path);
	if (!file.present) {
		kprint("File not present!\n");
	}
	uint32_t file_size = file.size;
	kprint("file_size: ");
	kprint_int(file_size, 10);
	kprint(".\n");
	void *contents = kmalloc(file_size);
	file.read(&file, 0, file_size, contents);
	struct elf_header *header = contents;
	kprint("ELF header:\n  Signature: 0x");
	kprint_int(header->signature, 16);
	kprint(".\n  ISA: 0x");
	kprint_int(header->isa, 16);
	kprint(".\n  Flags: 0x");
	kprint_int(header->flags, 16);
	kprint(".\n");
	struct scroll *first_scr = 0;
	struct scroll *current_scr = 0;
	for (uint32_t offset = header->program_table_offset;
			offset < header->program_table_offset + (header->program_table_length * header->program_table_entry_size);
			offset += header->program_table_entry_size) {
		struct elf_program_header *prog_header = contents + offset;
		if (prog_header->type != PT_LOAD) {
			continue;
		}
		uint32_t first_page = prog_header->p_vaddr - (prog_header->p_vaddr % PAGE_SIZE);
		uint32_t vaddr_end = prog_header->p_vaddr + prog_header->p_memsz;
		uint32_t limit_page = vaddr_end + PAGE_SIZE - (vaddr_end % PAGE_SIZE);
		for (uint32_t p = first_page; p < limit_page; p += PAGE_SIZE) {
			if (first_scr) {
				current_scr->next = kmalloc(sizeof(struct scroll));
				current_scr = current_scr->next;
			} else {
				first_scr = kmalloc(sizeof(struct scroll));
				current_scr = first_scr;
			}
			current_scr->vaddr = p;
			current_scr->type = SCROLL_ALIGNED;
			current_scr->aligned_backend.page = (uintptr_t)kpage_alloc();
			current_scr->size = PAGE_SIZE;
			current_scr->next = 0;
		}
	}
	lock_scheduler();
	load_user_call_info((func_ptr_t)(uintptr_t)header->entry_point, argc, argv);
	kprint("Initializing context...\n");
	create_context(enter_ring3, 1, true, first_scr);
	kprint("Context initialized.\n");
	uint8_t *data = kmalloc_aligned();
	current_scr = first_scr;
	for (uint32_t offset = header->program_table_offset;
			offset < header->program_table_offset + (header->program_table_length * header->program_table_entry_size);
			offset += header->program_table_entry_size) {
		struct elf_program_header *prog_header = contents + offset;
		if (prog_header->type != PT_LOAD) {
			continue;
		}
		uint32_t first_page = prog_header->p_vaddr - (prog_header->p_vaddr % PAGE_SIZE);
		uint32_t vaddr_end = prog_header->p_vaddr + prog_header->p_memsz;
		uint32_t limit_page = vaddr_end + PAGE_SIZE - (vaddr_end % PAGE_SIZE);
		uint32_t data_offset = prog_header->p_vaddr - first_page;
		uint32_t page_count = (limit_page - first_page) / PAGE_SIZE;
		uint32_t file_offset = prog_header->p_offset;
		kprint("Mapping from 0x");
		kprint_int(file_offset, 16);
		kprint("-0x");
		kprint_int(file_offset + prog_header->p_filesz, 16);
		kprint(" to 0x");
		kprint_int(current_scr->aligned_backend.page, 16);
		kprint("-0x");
		kprint_int(current_scr->aligned_backend.page + prog_header->p_filesz, 16);
		kprint(".\n");
		for (uint32_t i = 0; i < page_count; i++) {
			map_page((uintptr_t)data, current_scr->aligned_backend.page);
			while (file_offset - prog_header->p_offset < prog_header->p_filesz && data_offset < PAGE_SIZE) {
				data[data_offset] = ((uint8_t*)contents)[file_offset];
				data_offset++;
				file_offset++;
			}
			while (data_offset - prog_header->p_offset < prog_header->p_memsz && data_offset < PAGE_SIZE) {
				data[data_offset] = 0;
				data_offset++;
				file_offset++;
			}
			data_offset = 0;
			current_scr = current_scr->next;
		}
	}
	unmap_page((uintptr_t)data);
	kfree(contents);
	kfree(data);
	unlock_scheduler();
}
