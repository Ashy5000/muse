#include <muse/psf.h>
#include <muse/sync.h>
#include <muse/text.h>
#include <muse/vga.h>

struct vga_cursor {
	unsigned int x;
	unsigned int y;
};

struct vga_cursor cursor;
struct lock_reentrant console_lock;
extern struct vga_framebuffer fb;
extern const char psf_file[];
#define CONSOLE_W (fb.width / ((struct psf_font *)psf_file)->width)
#define CONSOLE_H (fb.height / ((struct psf_font *)psf_file)->height)

void init_console(struct multiboot_tag_framebuffer *tag_fb) {
	console_lock.cnt   = 0;
	console_lock.owner = 0;
	init_vga(tag_fb);
}

void console_put_char(char c, color_t color) {
	lock_reentrant_acquire(&console_lock);
	if (c == '\n') {
		cursor.x = 0;
		if (cursor.y < CONSOLE_H - 1) {
			cursor.y++;
		} else {
			scroll(((struct psf_font *)psf_file)->height);
		}
		return;
	}
	psf_put_char(c, cursor.x, cursor.y, color);
	if (cursor.x < CONSOLE_W - 1) {
		cursor.x++;
	} else {
		console_put_char('\n', 0);
	}
	lock_reentrant_release(&console_lock);
}

void kprint_fancy(const char *str, color_t color) {
	lock_reentrant_acquire(
	    &console_lock); /* Keep the console transaction atomic */
	int i = 0;
	while (str[i] != 0) {
		console_put_char(str[i], color);
		i++;
	}
	lock_reentrant_release(&console_lock);
}

#define kprint(S) kprint_fancy(S, 0xFFFFFF)

unsigned int nth_digit(unsigned int x, unsigned int digit, unsigned int base) {
	for (uint32_t i = 0; i < digit; i++) {
		x /= base;
	}
	return x % base;
}

void kprint_int_fancy(int x, int base, color_t color) {
	lock_reentrant_acquire(&console_lock);
	if (x == 0) {
		console_put_char('0', color);
		return;
	}
	int found_nonzero = 0;
	for (int i = 0; i < base; i++) {
		unsigned int digit = nth_digit(x, base - 1 - i, base);
		if (digit != 0) {
			found_nonzero = 1;
		}
		if (found_nonzero) {
			if (digit < 10) {
				console_put_char('0' + digit, color);
			} else {
				console_put_char('A' + digit - 10, color);
			}
		}
	}
	lock_reentrant_release(&console_lock);
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

void kprint_int_full_fancy(int x, int base, color_t color) {
	lock_reentrant_acquire(&console_lock);
	for (int i = 0; i < base; i++) {
		if (!inrange(base, base - 1 - i)) {
			continue;
		}
		unsigned int digit = nth_digit(x, base - 1 - i, base);
		if (digit < 10) {
			console_put_char('0' + digit, color);
		} else {
			console_put_char('A' + digit - 10, color);
		}
	}
	lock_reentrant_release(&console_lock);
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

void kvprintf_fancy(const char *fmt, color_t color, va_list args) {
	lock_reentrant_acquire(&console_lock);
	while (*fmt) {
		if (*fmt == '%') {
			fmt++;
			enum fmt_specifier spc = consume_specifier(fmt);
			switch (spc) {
			case FMT_SPC_NONE:
				console_put_char('%', color);
				console_put_char(*fmt, color);
				break;
			case FMT_SPC_STR:
				kprint_fancy(va_arg(args, const char *), color);
				break;
			case FMT_SPC_INT:
				kprint_int_fancy(va_arg(args, int), 10, color);
				break;
			case FMT_SPC_HEX:
				kprint_fancy("0x", color);
				kprint_int_fancy(va_arg(args, int), 16, color);
				break;
			}
		} else {
			console_put_char(*fmt, color);
		}
		fmt++;
	}
	lock_reentrant_release(&console_lock);
}

void kprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	kvprintf(fmt, args);
	va_end(args);
}

void kprintf_fancy(const char *fmt, color_t color, ...) {
	va_list args;
	va_start(args, color);
	kvprintf_fancy(fmt, color, args);
	va_end(args);
}
