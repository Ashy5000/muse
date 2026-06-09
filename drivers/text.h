#ifndef TEXT_H
#define TEXT_H

#include "../kernel/multiboot.h"
#include "vga.h"
#include <stdarg.h>

void init_console(struct multiboot_tag_framebuffer *tag_fb);
void console_put_char(char c, color_t color);
void kprint_int_fancy(int x, int base, color_t color);
#define kprint_int(X, B) kprint_int_fancy(X, B, 0xFFFFFF)
void kprint_int_full_fancy(int x, int base, color_t color);
#define kprint_int_full(X, B) kprint_int_full_fancy(X, B, 0xFFFFFF)
#define kvprintf(F, A)        kvprintf_fancy(F, 0xFFFFFF, A)
void kvprintf_fancy(const char *fmt, color_t color, va_list args);
void kprintf(const char *fmt, ...);
void kprintf_fancy(const char *fmt, color_t color, ...);

#endif
