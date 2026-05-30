#include "syscall.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

FILE *stdin;
FILE *stdout;
FILE *stderr;

#define MODE_READ     1
#define MODE_WRITE    2
#define MODE_APPEND   4
#define MODE_TRUNCATE 8
#define MODE_CREATE   16

unsigned long int syscall_transfer(FILE *f, long int size, unsigned char *res,
                                   bool write) {
	int fd = f->fd;
	if (write) {
		fd = -fd;
	}
	return muse_syscall(3, fd, size, (uintptr_t)res);
}

fpos_t syscall_seek(FILE *f, long int offset, int whence) {
	return muse_syscall(4, f->fd, offset, whence);
}

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
	f->pos  = syscall_seek(f, 0, SEEK_CUR); /* Syscall returns new offset */
	f->bfr_size = 0;
	f->bfr_cap  = BUFSIZ;
	f->bfr      = malloc(f->bfr_size);
	return f;
}

/* TODO: Implement locks on all file I/O operations. */

int fgetc(FILE *stream) {
	/* TODO: Flush the buffer. */
	unsigned char c;

	unsigned long int chars_read = syscall_transfer(stream, 1, &c, false);
	stream->pos += chars_read;
	if (chars_read == 1) {
		return c;
	}
	return EOF;
}

int fflush(FILE *stream) {
	unsigned long int chars_written =
	    syscall_transfer(stream, stream->bfr_size, stream->bfr, true);
	stream->pos += chars_written;
	if (chars_written == stream->bfr_size) {
		stream->bfr_size = 0;
		return 0;
	}
	return EOF;
}

int fputc(int c, FILE *stream) {
	if (!stream->bfr) {
		unsigned long int chars_written =
		    syscall_transfer(stream, 1, (unsigned char *)&c, true);
		stream->pos += chars_written;
		if (chars_written == 1) {
			return 0;
		}
		return EOF;
	}
	stream->bfr[stream->bfr_size++] = c;
	if (stream->bfr_size == stream->bfr_cap) {
		return fflush(stream);
	}
	return 0;
}

void init_io() {
	stderr           = malloc(sizeof(*stderr));
	stderr->fd       = 2;
	stderr->bfr_size = 0;
	stderr->bfr_cap  = 0;
	stderr->bfr      = 0;
}
