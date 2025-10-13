#include "../kernel/io.h"
#include "text.h"

#include <stdbool.h>

int shift_held = 0;

unsigned char keycode_to_char_lower(unsigned char keycode) {
	switch (keycode) {
		case 0x01: return 0; // Escape
		case 0x02: return '1';
		case 0x03: return '2';
		case 0x04: return '3';
		case 0x05: return '4';
		case 0x06: return '5';
		case 0x07: return '6';
		case 0x08: return '7';
		case 0x09: return '8';
		case 0x0A: return '9';
		case 0x0B: return '0';
		case 0x0C: return '-';
		case 0x0D: return '=';
		case 0x0E: return 255; // Backspace
		case 0x0F: return '	';
		case 0x10: return 'q';
		case 0x11: return 'w';
		case 0x12: return 'e';
		case 0x13: return 'r';
		case 0x14: return 't';
		case 0x15: return 'y';
		case 0x16: return 'u';
		case 0x17: return 'i';
		case 0x18: return 'o';
		case 0x19: return 'p';
		case 0x1A: return '[';
		case 0x1B: return ']';
		case 0x1C: return '\n';
		case 0x1D: return 0; // Left control
		case 0x1E: return 'a';
		case 0x1F: return 's';
		case 0x20: return 'd';
		case 0x21: return 'f';
		case 0x22: return 'g';
		case 0x23: return 'h';
		case 0x24: return 'j';
		case 0x25: return 'k';
		case 0x26: return 'l';
		case 0x27: return ';';
		case 0x28: return '\'';
		case 0x29: return '`';
		case 0x2A: // Left shift pressed
			   shift_held++;
			   return 0;
		case 0x2B: return '\\';
		case 0x2C: return 'z';
		case 0x2D: return 'x';
		case 0x2E: return 'c';
		case 0x2F: return 'v';
		case 0x30: return 'b';
		case 0x31: return 'n';
		case 0x32: return 'm';
		case 0x33: return ',';
		case 0x34: return '.';
		case 0x35: return '/';
		case 0x36:
			   // Right shift pressed
			   shift_held++;
			   return 0;
		case 0x39: return ' ';
		case 0xAA:
			   // Left shift released
			   shift_held--;
			   return 0;
		case 0xB6:
			   // Right shift released
			   shift_held--;
			   return 0;
		default: return 0;
	}
}

unsigned char keycode_to_char(unsigned char keycode) {
	unsigned char lower = keycode_to_char_lower(keycode);
	if (shift_held == 0) {
		return lower;
	}
	if (lower >= 'a' && lower <= 'z') {
		return lower + ('A' - 'a');
	}
	switch (lower) {
		case '`': return '~';
		case '1': return '!';
		case '2': return '@';
		case '3': return '#';
		case '4': return '$';
		case '5': return '%';
		case '6': return '^';
		case '7': return '&';
		case '8': return '*';
		case '9': return '(';
		case '0': return ')';
		case '-': return '_';
		case '=': return '+';
		case '[': return '{';
		case ']': return '}';
		case '\\': return '|';
		case ';': return ':';
		case '\'': return '"';
		case ',': return '<';
		case '.': return '>';
		case '/': return '?';
		default: return lower;
	}
}

bool keyboard_locked = false;

void handle_keypress(void) {
	while (keyboard_locked) {}
	keyboard_locked = true;
	outb(0x20, 0x20);
	io_wait();
	outb(0xA0, 0x20);
	io_wait();
	unsigned char scan_code = inb(0x60);
	unsigned char character = keycode_to_char(scan_code);
	if (character == 255) {
		kdel_char();
	} else if (character != 0) {
		kput_char(character);
	}
	__asm__ volatile ("sti");
	keyboard_locked = false;
}
