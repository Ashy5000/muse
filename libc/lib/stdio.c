#include "syscall.h"
#include <stdio.h>
#include <stdlib.h>

#define MODE_READ     1
#define MODE_WRITE    2
#define MODE_APPEND   4
#define MODE_TRUNCATE 8
#define MODE_CREATE   16

FILE *fopen(const char *restrict filename, const char *restrict mode_str) {
	uint8_t mode = 0;
	if (mode_str[0] == 'r') {
		mode |= MODE_READ;
	}
	if (mode_str[0] == 'w') {
		mode |= MODE_WRITE | MODE_TRUNCATE | MODE_CREATE;
	}
	if (mode_str[0] == 'a') {
		mode |= MODE_WRITE | MODE_APPEND | MODE_CREATE;
	}
	if (mode_str[1] == '+' || mode_str[2] == '+') {
		mode |= MODE_READ | MODE_WRITE;
	}
	if (mode_str[1] == 'x' || mode_str[2] == 'x' || mode_str[3] == 'x') {
		mode &= ~MODE_TRUNCATE;
	}
	int fd = muse_syscall(2, (uintptr_t)filename, mode, 0);
	if (fd < 0) {
		return 0;
	}
	FILE *f = malloc(sizeof(*f));
	f->fd   = fd;
	f->pos  = 0; /* TODO: Request this information from the kernel. */
	return f;
}

/* TODO: Implement locks on all file I/O operations. */

int getc(FILE *stream) {
	unsigned char c;
	uint32_t chars_read = muse_syscall(3, stream->fd, 1, (uintptr_t)&c);
	if (chars_read) {
		return c;
	}
	return EOF;
}
