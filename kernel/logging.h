#ifndef LOGGING_H
#define LOGGING_H

#include <stdarg.h>

enum log_level {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

enum log_class {
	LOG_ACPI,
	LOG_ALLOC,
	LOG_APIC,
	LOG_EXCEPTION,
	LOG_KERNEL,
	LOG_GPT,
	LOG_MEM,
	LOG_PAGING,
	LOG_PCI,
	LOG_PIC,
	LOG_HPET,
	LOG_SYSCALL,
};

void vlog(enum log_level level, enum log_class lclass, const char *fmt, va_list args);
void log(enum log_level level, enum log_class lclass, const char *fmt, ...);

#endif
