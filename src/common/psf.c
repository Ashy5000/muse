#include <muse/psf.h>

#define PSF_MAGIC 0x864AB572

const char psf_file[] = {
#embed "../../deps/font.psf"
};

void psf_put_char(unsigned char c, unsigned int x_c, unsigned int y_c,
                  color_t color) {
	struct psf_font *font = (struct psf_font *)psf_file;
	if (font->flags || font->magic != PSF_MAGIC) {
		return; /* We don't support Unicode. */
	}
	size_t width     = font->width;
	size_t height    = font->height;
	unsigned int x_a = x_c * font->width;
	unsigned int y_a = y_c * font->height;
	size_t line_size =
	    (width + 7) / 8; /* Padded to an integral number of bytes */
	char *data =
	    (char *)(font) + font->header_size + (font->glyph_size * c);
	size_t offset = 0;
	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			if ((data[offset + (x / 8)] >> (7 - (x % 8))) & 1) {
				put_pixel(x + x_a, y + y_a, color);
			} else {
				put_pixel(x + x_a, y + y_a, 0x000000);
			}
		}
		offset += line_size;
	}
}
