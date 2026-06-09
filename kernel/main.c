#include "../drivers/text.h"
#include "acpi.h"
#include "apic.h"
#include "interrupts.h"
#include "io.h"
#include "logging.h"
#include "memory.h"
#include "multiboot.h"
#include "pic.h"

extern struct mmap_entry mmap_table[MMAP_CNT];

void trampoline_main(void *multiboot, uint32_t magic) {
	if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
		__asm__ volatile("hlt");
	}

	log(LOG_INFO, LOG_KERNEL, "Multiboot info struct located at %x.\n",
	    multiboot);

	struct multiboot_tag_framebuffer *tag_fb =
	    (struct multiboot_tag_framebuffer *)multiboot_find_tag(
		multiboot, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
	if (!tag_fb) {
		log(LOG_ERROR, LOG_KERNEL, "Framebuffer tag not present!\n");
		__asm__ volatile("hlt");
	}
	init_console(tag_fb);

	for (unsigned int i = 0; i < MMAP_CNT; i++) {
		mmap_table[i].available = false;
	}

	struct multiboot_tag_mmap *tag_mmap =
	    (struct multiboot_tag_mmap *)multiboot_find_tag(
		multiboot, MULTIBOOT_TAG_TYPE_MMAP);

	if (!tag_mmap) {
		log(LOG_ERROR, LOG_KERNEL, "No memory map tag found!\n");
		__asm__ volatile("hlt");
	}

	struct multiboot_mmap_entry *entry =
	    (struct multiboot_mmap_entry *)(uintptr_t)tag_mmap->entries;
	uint32_t table_idx = 0;
	while ((void *)entry < (void *)tag_mmap + tag_mmap->size) {
		log(LOG_INFO, LOG_MEM,
		    "CHUNK FOUND - Addr: %x | Size: %x | Type: %i\n",
		    entry->addr, entry->len, entry->type);
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
			mmap_table[table_idx].available = true;
			mmap_table[table_idx].addr      = entry->addr;
			mmap_table[table_idx].size      = entry->len;
			table_idx++;
			if (table_idx == MMAP_CNT) {
				break;
			}
		}
		entry = (void *)entry + tag_mmap->entry_size;
	}

	struct multiboot_tag_old_acpi *tag_acpi =
	    (struct multiboot_tag_old_acpi *)multiboot_find_tag(
		multiboot, MULTIBOOT_TAG_TYPE_ACPI_OLD);
	if (!tag_acpi) {
		tag_acpi = (struct multiboot_tag_old_acpi *)multiboot_find_tag(
		    multiboot, MULTIBOOT_TAG_TYPE_ACPI_NEW);
		if (!tag_acpi) {
			log(LOG_ERROR, LOG_KERNEL, "No ACPI tag found!\n");
		}
	}

	init_acpi(tag_acpi);
	init_pic();
	init_apic();
	init_ioapic();
	init_idt();

	struct multiboot_tag_elf_sections *tag_elf =
	    (struct multiboot_tag_elf_sections *)multiboot_find_tag(
		multiboot, MULTIBOOT_TAG_TYPE_ELF_SECTIONS);
	if (!tag_elf) {
		log(LOG_ERROR, LOG_KERNEL, "No ELF tag found!\n");
	}
	// init_memory(tag_elf);

	for (;;) {
		__asm__("hlt");
	}
}
