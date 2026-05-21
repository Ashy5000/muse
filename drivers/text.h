#ifndef TEXT_H
#define TEXT_H

#include <stdarg.h>

enum vga_text_color {
	VGA_BLACK,
	VGA_BLUE,
	VGA_GREEN,
	VGA_CYAN,
	VGA_RED,
	VGA_MAGENTA,
	VGA_BROWN,
	VGA_GRAY_LIGHT,
	VGA_GRAY_DARK,
	VGA_BLUE_LIGHT,
	VGA_GREEN_LIGHT,
	VGA_CYAN_LIGHT,
	VGA_RED_LIGHT,
	VGA_MAGENTA_LIGHT,
	VGA_YELLOW,
	VGA_WHITE,
};

void init_console(void);
void kdel_char(void);
void kput_char(char c);
void kprint_hex(char *data, int len);
void kprint_int(int x, int base);
void kprint_int_full(int x, int base);
void kvprintf(const char *fmt, va_list args);
void kvprintf_fancy(const char *fmt, enum vga_text_color fg, enum vga_text_color bg, va_list args);
void kprintf(const char *fmt, ...);
void kprintf_fancy(const char *fmt, enum vga_text_color fg, enum vga_text_color bg, ...);

#endif
