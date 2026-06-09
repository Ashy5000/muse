#ifndef PSF_H
#define PSF_H

#include "vga.h"
#include <stdint.h>

struct psf_font {
	uint32_t magic;
	uint32_t version;
	uint32_t header_size;
	uint32_t flags;
	uint32_t num;
	uint32_t glyph_size;
	uint32_t height;
	uint32_t width;
};

void psf_put_char(unsigned char c, unsigned int x_c, unsigned int y_c,
                  color_t color);

#endif
