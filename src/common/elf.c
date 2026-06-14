#include <muse/alloc.h>
#include <muse/context.h>
#include <muse/elf.h>
#include <muse/logging.h>
#include <muse/scheduler.h>
#include <muse/scroll.h>
#include <muse/userspace.h>
#include <muse/utils.h>
#include <muse/vfs.h>

#define PT_LOAD 1

struct elf_info parse_elf(char *path) {
	struct elf_info res;
	res.present            = false;
	struct vfs_inode *file = vfs_open(path);
	if (!file) {
		return res;
	}
	uint32_t file_size = file->size;
	log(LOG_INFO, LOG_MEM, "ELF executable found with size %x.\n",
	    file_size);
	res.contents = kmalloc(file_size);
	if (file->transfer(file, 0, file_size, res.contents, DIR_READ) !=
	    file_size) {
		kfree(res.contents);
		return res;
	}
	res.present                = true;
	res.header                 = res.contents;
	res.first_scr              = 0;
	struct scroll *current_scr = 0;
	res.limit                  = 0;
	for (uint32_t offset = res.header->program_table_offset;
	     offset < res.header->program_table_offset +
	                  (res.header->program_table_length *
	                   res.header->program_table_entry_size);
	     offset += res.header->program_table_entry_size) {
		struct elf_program_header *prog_header = res.contents + offset;
		if (prog_header->type != PT_LOAD) {
			continue;
		}
		log(LOG_INFO, LOG_MEM, "Elf section: %x->%x.\n",
		    prog_header->p_vaddr,
		    prog_header->p_vaddr + prog_header->p_memsz);
		uint32_t first_page =
		    prog_header->p_vaddr - (prog_header->p_vaddr % PAGE_SIZE);
		uint32_t vaddr_end =
		    prog_header->p_vaddr + prog_header->p_memsz;
		uint32_t limit_page =
		    vaddr_end + PAGE_SIZE - (vaddr_end % PAGE_SIZE);
		for (uint32_t p = first_page; p < limit_page; p += PAGE_SIZE) {
			if (res.first_scr) {
				current_scr->next =
				    kmalloc(sizeof(struct scroll));
				current_scr = current_scr->next;
			} else {
				res.first_scr = kmalloc(sizeof(struct scroll));
				current_scr   = res.first_scr;
			}
			current_scr->vaddr = p;
			current_scr->type  = SCROLL_ALIGNED;
			current_scr->aligned_backend.page =
			    (uintptr_t)kpage_alloc();
			current_scr->size = PAGE_SIZE;
			current_scr->next = 0;
		}
		if (limit_page > res.limit) {
			res.limit = limit_page;
		}
	}
	return res;
}

void load_elf_data(struct elf_info elf) {
	volatile uint8_t *data     = kmalloc_aligned();
	struct scroll *current_scr = elf.first_scr;
	for (uint32_t offset = elf.header->program_table_offset;
	     offset < elf.header->program_table_offset +
	                  (elf.header->program_table_length *
	                   elf.header->program_table_entry_size);
	     offset += elf.header->program_table_entry_size) {
		struct elf_program_header *prog_header = elf.contents + offset;
		if (prog_header->type != PT_LOAD) {
			continue;
		}
		uint32_t first_page =
		    prog_header->p_vaddr - (prog_header->p_vaddr % PAGE_SIZE);
		uint32_t vaddr_end =
		    prog_header->p_vaddr + prog_header->p_memsz;
		uint32_t limit_page  = ALIGN_PG_UP(vaddr_end);
		uint32_t data_offset = prog_header->p_vaddr - first_page;
		uint32_t page_count  = (limit_page - first_page) / PAGE_SIZE;
		uint32_t file_offset = prog_header->p_offset;
		for (uint32_t i = 0; i < page_count; i++) {
			map_page((uintptr_t)data,
			         current_scr->aligned_backend.page);
			while (file_offset - prog_header->p_offset <
			           prog_header->p_filesz &&
			       data_offset < PAGE_SIZE) {
				data[data_offset] =
				    ((uint8_t *)elf.contents)[file_offset];
				data_offset++;
				file_offset++;
			}
			while (data_offset - prog_header->p_offset <
			           prog_header->p_memsz &&
			       data_offset < PAGE_SIZE) {
				data[data_offset] = 0;
				data_offset++;
			}
			data_offset = 0;
			current_scr = current_scr->next;
		}
	}
	unmap_page((uintptr_t)data);
	kfree(elf.contents);
	kfree((void *)data);
}

struct scroll
reserve_multiboot_kernel(struct multiboot_tag_elf_sections *tag_elf) {
	struct elf_section_header *section_table =
	    (struct elf_section_header *)&tag_elf->sections;
	uintptr_t kernel_start = -1;
	uintptr_t kernel_end   = 0;
	char *string_table =
	    (char *)(uintptr_t)(section_table[tag_elf->shndx].sh_addr);
	for (unsigned int i = 0; i < tag_elf->num; i++) {
		if (!section_table[i].sh_addr || !section_table[i].sh_type ||
		    !(section_table[i].sh_flags & 0x2)) {
			continue;
		}
		log(LOG_INFO, LOG_MEM, "Found section %s from %x->%x.\n",
		    string_table + section_table[i].sh_name,
		    section_table[i].sh_addr,
		    section_table[i].sh_addr + section_table[i].sh_size);
		kernel_start = MIN(section_table[i].sh_addr, kernel_start);
		kernel_end =
		    MAX(section_table[i].sh_addr + section_table[i].sh_size,
		        kernel_end);
	}
	log(LOG_INFO, LOG_MEM, "Reserving kernel memory from %x->%x.\n",
	    kernel_start, kernel_end);
	paddr_t first_pg = ALIGN_PG_DOWN(kernel_start);
	paddr_t last_pg  = ALIGN_PG_DOWN(kernel_end);
	for (paddr_t pg_start = first_pg; pg_start <= last_pg;
	     pg_start += PAGE_SIZE) {
		kpage_set_status(pg_start, false);
	}
	struct scroll res;
	res.vaddr                = ALIGN_PG_DOWN(kernel_start);
	res.aligned_backend.page = ALIGN_PG_DOWN(kernel_start);
	res.size                 = ALIGN_PG_UP(kernel_end) - res.vaddr;
	res.type                 = SCROLL_ALIGNED;
	return res;
}
