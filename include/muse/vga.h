#ifndef VGA_H
#define VGA_H

#include <muse/multiboot.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct vga_framebuffer {
	uint8_t *bfr;
	size_t width;
	size_t height;
	size_t pitch;
};

typedef uint32_t color_t;

void init_vga(struct multiboot_tag_framebuffer *tag_fb);
void put_pixel(unsigned int x, unsigned int y, color_t c);
void fill_rect(unsigned int x_a, unsigned int y_a, unsigned int w,
               unsigned int h, color_t c);
void scroll(unsigned int d_y);

#endif
