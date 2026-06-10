#include <muse/paging.h>
#include <muse/scroll.h>
#include <muse/vga.h>

struct vga_framebuffer fb;
struct scroll fb_scr;

void put_pixel(unsigned int x, unsigned int y, color_t c) {
	size_t offset      = x * 3 + y * fb.pitch;
	fb.bfr[offset]     = c & 0xFF;
	fb.bfr[offset + 1] = (c >> 8) & 0xFF;
	fb.bfr[offset + 2] = (c >> 16) & 0xFF;
	return;
}

void fill_rect(unsigned int x_a, unsigned int y_a, unsigned int w,
               unsigned int h, color_t c) {
	size_t offset = x_a * 3 + y_a * fb.pitch;
	for (unsigned int y = 0; y < h; y++) {
		for (unsigned int x = 0; x < w; x++) {
			fb.bfr[offset + x * 3]     = c & 0xFF;
			fb.bfr[offset + x * 3 + 1] = (c >> 8) & 0xFF;
			fb.bfr[offset + x * 3 + 2] = (c >> 16) & 0xFF;
		}
		offset += fb.pitch;
	}
}

/* TODO: Error handling */
void init_vga(struct multiboot_tag_framebuffer *tag_fb) {
	fb.bfr    = (uint8_t *)(uintptr_t)tag_fb->common.framebuffer_addr;
	fb.width  = tag_fb->common.framebuffer_width;
	fb.height = tag_fb->common.framebuffer_height;
	fb.pitch  = tag_fb->common.framebuffer_pitch;
	for (uint32_t i = 0; i < fb.pitch * fb.width; i++) {
		fb.bfr[i] = 0x00;
	}
	fb_scr.type                 = SCROLL_ALIGNED;
	fb_scr.vaddr                = tag_fb->common.framebuffer_addr;
	fb_scr.size                 = ALIGN_PG_UP(fb.pitch * fb.width);
	fb_scr.aligned_backend.page = tag_fb->common.framebuffer_addr;
	reserve_scroll(&fb_scr);
}

void scroll(unsigned int d_y) {
	memcpy(fb.bfr, fb.bfr + (fb.pitch * d_y),
	       (fb.height - (fb.height % d_y)) * fb.pitch);
	for (unsigned int y = fb.height - (fb.height % d_y) - d_y;
	     y < fb.height; y++) {
		for (unsigned int x = 0; x < fb.width; x++) {
			fb.bfr[(y * fb.pitch) + x] = 0;
		}
	}
	return;
}
