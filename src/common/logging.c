#include <muse/logging.h>
#include <muse/text.h>

#define DEF_LOG_LEVEL(CLASS, LEVEL)                                            \
	case CLASS:                                                            \
		if (level < LEVEL) {                                           \
			return;                                                \
		}                                                              \
		break;

#define DEF_LOG_LEVELS(CASES)                                                  \
	switch (lclass) {                                                      \
		CASES                                                          \
	default:                                                               \
		if (level < LOG_INFO) {                                        \
			return;                                                \
		}                                                              \
	}

bool logging_enabled = true;

void vlog(enum log_level level, enum log_class lclass, const char *fmt,
          va_list args) {
	if (!logging_enabled) {
		return;
	}

	DEF_LOG_LEVELS();

	switch (level) {
	case LOG_DEBUG:
		kprintf_fancy("[DBG] ", 0xFFFFFF);
		break;
	case LOG_INFO:
		kprintf_fancy("[INF] ", 0x33CCFF);
		break;
	case LOG_WARN:
		kprintf_fancy("[WRN] ", 0xFFFF1A);
		break;
	case LOG_ERROR:
		kprintf_fancy("[ERR] ", 0xFF3300);
		break;
	default:
		kprintf_fancy("[???] ", 0xFF3399);
	}
	switch (lclass) {
	case LOG_ACPI:
		kprintf("acpi: ");
		break;
	case LOG_ALLOC:
		kprintf("alloc: ");
		break;
	case LOG_APIC:
		kprintf("apic: ");
		break;
	case LOG_ATA:
		kprintf("ata: ");
		break;
	case LOG_EXCEPTION:
		kprintf("exception: ");
		break;
	case LOG_EXT2:
		kprintf("ext2: ");
		break;
	case LOG_KERNEL:
		kprintf("kernel: ");
		break;
	case LOG_GPT:
		kprintf("gpt: ");
		break;
	case LOG_MEM:
		kprintf("mem: ");
		break;
	case LOG_PAGING:
		kprintf("paging: ");
		break;
	case LOG_PCI:
		kprintf("pci: ");
		break;
	case LOG_PIC:
		kprintf("pic: ");
		break;
	case LOG_PS2:
		kprintf("ps/2: ");
		break;
	case LOG_HPET:
		kprintf("hpet: ");
		break;
	case LOG_SYSCALL:
		kprintf("syscall: ");
		break;
	case LOG_TERM:
		kprintf("term: ");
		break;
	default:
		kprintf("???: ");
	}
	kvprintf(fmt, args);
}

void log(enum log_level level, enum log_class lclass, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vlog(level, lclass, fmt, args);
	va_end(args);
}
