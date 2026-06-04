#include "syscall.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *stdin;
FILE *stdout;
FILE *stderr;

#define MODE_READ     1
#define MODE_WRITE    2
#define MODE_APPEND   4
#define MODE_TRUNCATE 8
#define MODE_CREATE   16

unsigned long int syscall_read(FILE *f, long int size, unsigned char *res) {
	return muse_syscall(3, f->fd, size, (uintptr_t)res);
}

unsigned long int syscall_write(FILE *f, long int size,
                                const unsigned char *res) {
	return muse_syscall(3, -f->fd, size, (uintptr_t)res);
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
	f->bfr_size      = 0;
	f->bfr_cap       = BUFSIZ;
	f->bfr           = malloc(f->bfr_size);
	f->line_buffered = false;
	return f;
}

/* TODO: Implement locks on all file I/O operations. */

int fflush(FILE *stream) {
	unsigned long int chars_written =
	    syscall_write(stream, stream->bfr_size, stream->bfr);
	stream->pos += chars_written;
	if (chars_written == stream->bfr_size) {
		stream->bfr_size = 0;
		return 0;
	}
	return EOF;
}

int fclose(FILE *stream) {
	int res = 0;
	if (fflush(stream) == EOF) {
		res = EOF;
	}
	if (stream->bfr) {
		free(stream->bfr);
	}
	if ((int)muse_syscall(5, stream->fd, 0, 0) < 0) {
		return EOF;
	}
	return res;
}

int fgetc(FILE *stream) {
	fflush(stream);

	unsigned char c;
	unsigned long int chars_read = syscall_read(stream, 1, &c);
	stream->pos += chars_read;
	if (chars_read == 1) {
		return c;
	}
	return EOF;
}

int fputc(int c, FILE *stream) {
	if (!stream->bfr) {
		unsigned long int chars_written =
		    syscall_write(stream, 1, (unsigned char *)&c);
		stream->pos += chars_written;
		if (chars_written == 1) {
			return c;
		}
		return EOF;
	}
	stream->bfr[stream->bfr_size++] = c;
	if (stream->bfr_size == stream->bfr_cap ||
	    (stream->line_buffered && c == '\n')) {
		return fflush(stream);
	}
	return c;
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb,
              FILE *restrict stream) {
	if (!stream->bfr) {
		/* If the stream isn't buffered, minimize syscalls by only
		 * writing once. */
		unsigned long int chars_written = syscall_write(
		    stream, size * nmemb, (const unsigned char *)ptr);
		stream->pos += chars_written;
		return chars_written / size;
	}
	size_t i = 0;
	for (; i < size * nmemb; i++) {
		if (fputc(((char *)ptr)[i], stream) == EOF) {
			break;
		}
	}
	return (i + 1) / size;
}

int fputs(const char *restrict s, FILE *restrict stream) {
	return fwrite((const void *)s, sizeof(char), strlen(s), stream);
}

size_t fread(void *restrict ptr, size_t size, size_t nmemb,
             FILE *restrict stream) {
	fflush(stream);
	unsigned long int chars_read =
	    syscall_read(stream, size * nmemb, (unsigned char *)ptr);
	stream->pos += chars_read;
	return chars_read / size;
}

void init_io() {
	stdin                 = malloc(sizeof(*stdin));
	stdin->fd             = 0;
	stdin->bfr_size       = 0;
	stdin->bfr_cap        = 0;
	stdin->bfr            = 0;
	stdin->line_buffered  = true;
	stdout                = malloc(sizeof(*stdout));
	stdout->fd            = 1;
	stdout->bfr_size      = 0;
	stdout->bfr_cap       = BUFSIZ;
	stdout->bfr           = malloc(stdout->bfr_cap);
	stdout->line_buffered = true;
	stderr                = malloc(sizeof(*stderr));
	stderr->fd            = 2;
	stderr->bfr_size      = 0;
	stderr->bfr_cap       = 0;
	stderr->bfr           = 0;
	stderr->line_buffered = false;
}

void uninit_io() {
	free(stdin);
	free(stdout->bfr);
	free(stdout);
	free(stderr);
}
