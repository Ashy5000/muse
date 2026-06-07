#include "text.h"
#include "../kernel/paging.h"
#include "../kernel/sync.h"

struct vga_cursor {
	int x;
	int y;
};

struct vga_cursor kcursor;
struct lock_reentrant console_lock;

char *video_memory = (char *)0xb8000;
struct scroll vmem_scr;

void init_console(void) {
	vmem_scr.vaddr = ALIGN_PG_DOWN(video_memory);
	vmem_scr.size  = ALIGN_PG_UP(80 * 24 * 2);
	reserve_scroll(&vmem_scr);
	console_lock.cnt   = 0;
	console_lock.owner = 0;
	lock_reentrant_acquire(&console_lock);
	for (int i = 0; i < 80 * 25; i++) {
		video_memory[i * 2] = 0;
	}
	lock_reentrant_release(&console_lock);
}

void kscroll(void) {
	lock_reentrant_acquire(&console_lock);
	memcpy(video_memory, video_memory + 80 * 2, 80 * 2 * 24);
	for (int i = 0; i < 80; i++) {
		video_memory[80 * 2 * 24 + i * 2] = 0;
	}
	lock_reentrant_release(&console_lock);
}

void kdel_char(void) {
	lock_reentrant_acquire(&console_lock);
	if (kcursor.x > 0) {
		kcursor.x--;
	} else {
		if (kcursor.y == 0) {
			return;
		}
		kcursor.x = 79;
		kcursor.y--;
	}
	video_memory[(kcursor.y * 80 + kcursor.x) * 2] = 0;
	lock_reentrant_release(&console_lock);
}

void kput_char(char c) {
	lock_reentrant_acquire(&console_lock);
	if (c == '\n') {
		kcursor.x = 0;
		if (kcursor.y < 24) {
			kcursor.y++;
		} else {
			kscroll();
		}
		return;
	}
	video_memory[(kcursor.y * 80 + kcursor.x) * 2] = c;
	if (kcursor.x < 79) {
		kcursor.x++;
	} else {
		kput_char('\n');
	}
	lock_reentrant_release(&console_lock);
}

void style_char(enum vga_text_color fg, enum vga_text_color bg) {
	lock_reentrant_acquire(&console_lock);
	video_memory[(kcursor.y * 80 + kcursor.x) * 2 + 1] = fg | (bg << 4);
	lock_reentrant_release(&console_lock);
}

void kprint_fancy(const char *str, enum vga_text_color fg,
                  enum vga_text_color bg) {
	lock_reentrant_acquire(
	    &console_lock); /* Keep the console transaction atomic */
	int i = 0;
	while (str[i] != 0) {
		style_char(fg, bg);
		kput_char(str[i]);
		i++;
	}
	lock_reentrant_release(&console_lock);
}

void kprint(const char *str) { kprint_fancy(str, VGA_WHITE, VGA_BLACK); }

char bin_to_hex(char x) {
	if (x < 10) {
		return '0' + x;
	}
	return 'A' + x - 10;
}

void kprint_hex(char *data, int len) {
	lock_reentrant_acquire(&console_lock);
	for (int i = 0; i < len; i++) {
		kput_char(bin_to_hex(data[i] >> 4));
		kput_char(bin_to_hex(data[i] & 0xf));
	}
	lock_reentrant_release(&console_lock);
}

unsigned int nth_digit(unsigned int x, unsigned int digit, unsigned int base) {
	for (uint32_t i = 0; i < digit; i++) {
		x /= base;
	}
	return x % base;
}

void kprint_int_fancy(int x, int base, enum vga_text_color fg,
                      enum vga_text_color bg) {
	lock_reentrant_acquire(&console_lock);
	if (x == 0) {
		style_char(fg, bg);
		kput_char('0');
		return;
	}
	int found_nonzero = 0;
	for (int i = 0; i < base; i++) {
		unsigned int digit = nth_digit(x, base - 1 - i, base);
		if (digit != 0) {
			found_nonzero = 1;
		}
		if (found_nonzero) {
			style_char(fg, bg);
			if (digit < 10) {
				kput_char('0' + digit);
			} else {
				kput_char('A' + digit - 10);
			}
		}
	}
	lock_reentrant_release(&console_lock);
}

void kprint_int(int x, int base) {
	kprint_int_fancy(x, base, VGA_WHITE, VGA_BLACK);
}

bool inrange(int base, int exp) {
	int res = 1;
	for (int i = 0; i < exp; i++) {
		if (res * base < res) {
			return false;
		}
		res *= base;
	}
	return true;
}

void kprint_int_full_fancy(int x, int base, enum vga_text_color fg,
                           enum vga_text_color bg) {
	lock_reentrant_acquire(&console_lock);
	for (int i = 0; i < base; i++) {
		if (!inrange(base, base - 1 - i)) {
			continue;
		}
		unsigned int digit = nth_digit(x, base - 1 - i, base);
		style_char(fg, bg);
		if (digit < 10) {
			kput_char('0' + digit);
		} else {
			kput_char('A' + digit - 10);
		}
	}
	lock_reentrant_release(&console_lock);
}

void kprint_int_full(int x, int base) {
	kprint_int_full_fancy(x, base, VGA_WHITE, VGA_BLACK);
}

enum fmt_specifier {
	FMT_SPC_NONE,
	FMT_SPC_STR,
	FMT_SPC_INT,
	FMT_SPC_HEX,
};

enum fmt_specifier consume_specifier(const char *fmt) {
	switch (*fmt) {
	case 's':
		return FMT_SPC_STR;
	case 'i':
		return FMT_SPC_INT;
	case 'x':
		return FMT_SPC_HEX;
	}
	return FMT_SPC_NONE;
}

void kvprintf_fancy(const char *fmt, enum vga_text_color fg,
                    enum vga_text_color bg, va_list args) {
	lock_reentrant_acquire(&console_lock);
	while (*fmt) {
		if (*fmt == '%') {
			fmt++;
			enum fmt_specifier spc = consume_specifier(fmt);
			switch (spc) {
			case FMT_SPC_NONE:
				style_char(fg, bg);
				kput_char('%');
				style_char(fg, bg);
				kput_char(*fmt);
				break;
			case FMT_SPC_STR:
				kprint_fancy(va_arg(args, const char *), fg,
				             bg);
				break;
			case FMT_SPC_INT:
				kprint_int_fancy(va_arg(args, int), 10, fg, bg);
				break;
			case FMT_SPC_HEX:
				kprint_fancy("0x", fg, bg);
				kprint_int_fancy(va_arg(args, int), 16, fg, bg);
				break;
			}
		} else {
			style_char(fg, bg);
			kput_char(*fmt);
		}
		fmt++;
	}
	lock_reentrant_release(&console_lock);
}

void kvprintf(const char *fmt, va_list args) {
	kvprintf_fancy(fmt, VGA_WHITE, VGA_BLACK, args);
}

void kprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	kvprintf(fmt, args);
	va_end(args);
}

void kprintf_fancy(const char *fmt, enum vga_text_color fg,
                   enum vga_text_color bg, ...) {
	va_list args;
	va_start(args, bg);
	kvprintf_fancy(fmt, fg, bg, args);
	va_end(args);
}
