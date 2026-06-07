#include "../drivers/hpet.h"
#include "../drivers/text.h"
#include "acpi.h"
#include "apic.h"
#include "context.h"
#include "interrupts.h"
#include "logging.h"
#include "multiboot.h"
#include "pic.h"
#include "syscall.h"
#include "term.h"
#include "userspace.h"

extern struct mmap_entry mmap_table[MMAP_CNT];

void main(struct multiboot_info *multiboot, uint32_t magic) {
	init_console();
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		log(LOG_ERROR, LOG_KERNEL, "Invalid multiboot magic number!\n");
		__asm__ volatile("cli; hlt");
	}

	if (!(multiboot->flags & MULTIBOOT_INFO_MEMORY)) {
		log(LOG_ERROR, LOG_KERNEL,
		    "Multiboot memory map not supplied!\n");
		__asm__ volatile("cli; hlt");
	}

	if (!(multiboot->flags & MULTIBOOT_INFO_ELF_SHDR)) {
		log(LOG_ERROR, LOG_KERNEL,
		    "ELF info not supplied via multiboot!\n");
		__asm__ volatile("cli; hlt");
	}

	for (unsigned int i = 0; i < MMAP_CNT; i++) {
		mmap_table[i].available = false;
	}

	uint32_t table_idx = 0;
	for (unsigned int i = 0;
	     i < multiboot->mmap_length / sizeof(struct multiboot_mmap_entry);
	     i++) {
		struct multiboot_mmap_entry *entry =
		    &((struct multiboot_mmap_entry *)(uintptr_t)
		          multiboot->mmap_addr)[i];
		log(LOG_INFO, LOG_MEM,
		    "CHUNK FOUND - Addr: %x | Size: %x | Type: %i\n",
		    entry->addr_low, entry->size_low, entry->type);
		if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
			mmap_table[table_idx].available = true;
			mmap_table[table_idx].addr      = entry->addr_low;
			mmap_table[table_idx].size      = entry->size_low;
			table_idx++;
			if (table_idx == MMAP_CNT) {
				break;
			}
		}
	}

	init_memory(&multiboot->u.elf_sec);
	// init_acpi();
	// init_hpet();
	// init_pic();
	// init_apic();
	// init_ioapic();
	// init_idt();
	// init_userspace();
	// init_scheduler();
	// init_syscalls();
	// init_root_term();

	for (;;) {
		__asm__("hlt");
	}
}
