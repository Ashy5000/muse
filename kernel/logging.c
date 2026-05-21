#include "logging.h"
#include "../drivers/text.h"

#define LOG_LEVEL LOG_INFO

void log(enum log_level level, enum log_class lclass, const char *fmt, ...) {
	if (level < LOG_LEVEL) {
		return;
	}
	switch (level) {
		case LOG_DEBUG:
			kprintf_fancy("[DBG] ", VGA_WHITE, VGA_BLACK);
			break;
		case LOG_INFO:
			kprintf_fancy("[INF] ", VGA_CYAN, VGA_BLACK);
			break;
		case LOG_WARN:
			kprintf_fancy("[WRN] ", VGA_YELLOW, VGA_BLACK);
			break;
		case LOG_ERROR:
			kprintf_fancy("[ERR] ", VGA_RED, VGA_BLACK);
			break;
		default:
			kprintf_fancy("[???] ", VGA_MAGENTA, VGA_BLACK);
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
		case LOG_EXCEPTION:
			kprintf("exception: ");
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
		case LOG_HPET:
			kprintf("hpet: ");
			break;
		default:
			kprintf("???: ");
	}
	va_list args;
	va_start(args, fmt);
	kvprintf(fmt, args);
	va_end(args);
}
