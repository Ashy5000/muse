#ifndef TERM_H
#define TERM_H

#include "sync.h"
#include "vfs.h"
#include <stddef.h>

#define TERM_BFR_HEIGHT 8
#define TERM_BFR_WIDTH  64

struct stdin_line {
	size_t len;
	char s[TERM_BFR_WIDTH];
};

struct stdin {
	size_t active_line; /* The index of the line being written to (the
	                       active line) */
	size_t bfr_offset;  /* The file position of the start of the buffer */
	struct stdin_line lines[TERM_BFR_HEIGHT];
	volatile struct lock_multi *lock;
};

void init_root_term();
void term_stdin_putc(struct vfs_inode *inode, char c);

extern struct vfs_inode *root_term;

#endif
