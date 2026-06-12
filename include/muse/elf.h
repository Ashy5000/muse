#ifndef ELF_H
#define ELF_H

#include <muse/hal.h>
#include <muse/int_types.h>
#include <muse/multiboot.h>
#include <muse/vfs.h>
#include <stdint.h>

struct elf_header {
	uint32_t signature;
	uint8_t word_size;
	uint8_t endianness;
	uint8_t header_version;
	uint8_t os_abi;
	char rsvd[8];
	uint16_t type;
	uint16_t isa;
	uint32_t elf_version;
	uint32_t entry_point;
	uint32_t program_table_offset;
	uint32_t section_table_offset;
	uint32_t flags;
	uint16_t header_size;
	uint16_t program_table_entry_size;
	uint16_t program_table_length;
	uint16_t section_table_entry_size;
	uint16_t section_table_length;
	uint16_t section_string_table_index;
};

struct elf_program_header {
	uint32_t type;
	uint32_t p_offset;
	uint32_t p_vaddr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t flags;
	uint32_t alignment;
};

struct elf_section_header {
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
};

struct elf_info {
	bool present;
	struct scroll *first_scr;
	vaddr_t limit;
	struct elf_header *header;
	void *contents;
};

struct elf_info parse_elf(char *path);
void load_elf_data(struct elf_info elf);

struct scroll
reserve_multiboot_kernel(struct multiboot_tag_elf_sections *tag_elf);

#endif
