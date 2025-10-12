#include "text.h"
#include "../kernel/memory.h"

struct vga_cursor {
	int x;
	int y;
};

struct vga_cursor kcursor;

char *video_memory = (char*) 0xb8000;

void kscroll() {
	kmemcpy(video_memory, video_memory + 80 * 2, 80 * 2 * 24);
	for (int i = 0; i < 80; i++) {
		video_memory[80 * 2 * 24 + i] = 0;
	}
}

void kput_char(char c) {
	if (c == '\n') {
		while (kcursor.x < 79) {
			video_memory[(kcursor.y * 80 + kcursor.x) * 2] = 0;
			kcursor.x++;
		}
		kcursor.x = 0;
		if (kcursor.y < 24) {
			kcursor.y++;
		} else {
			kscroll();
		}
		return;
	}
	video_memory[(kcursor.y * 80 + kcursor.x) * 2] = c;
	kcursor.x++;
}

void kprint(const char *str) {
	int i = 0;
	while (str[i] != 0) {
		kput_char(str[i]);
		i++;
	}
}

char bin_to_hex(char x) {
	if (x < 10) {
		return '0' + x;
	}
	return 'A' + x - 10;
}

void kprint_hex(char *data, int len) {
	for (int i = 0; i < len; i++) {
		kput_char(bin_to_hex(data[i] >> 4));
		kput_char(bin_to_hex(data[i] & 0xf));
	}
}

int nth_digit(int x, int digit) {
	for (int i = 0; i < digit; i++) {
		x /= 10;
	}
	x %= 10;
	return x;
}

void kprint_int(int x) {
	int found_nonzero = 0;
	for(int i = 0; i <= 9; i++) {
		int digit = nth_digit(x, 9 - i);
		if (digit != 0) {
			found_nonzero = 1;
		}
		if (found_nonzero) {
			kput_char('0' + digit);
		}
	}
	kput_char('\n');
}
