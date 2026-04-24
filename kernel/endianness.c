#include "endianness.h"

uint16_t invert_endianness_16(uint16_t x) {
	return ((x & 0xFF) << 8)
		| ((x & 0xFF00) >> 8);
}

uint32_t invert_endianness_32(uint32_t x) {
	return ((x & 0xFF) << 16)
		| ((x & 0xFF00) << 4)
		| ((x & 0xFF0000) >> 4)
		| ((x & 0xFF000000) >> 16);
}
