#ifndef SCROLL_H
#define SCROLL_H

#include "int_types.h"

enum scroll_type {
	SCROLL_FAILED,
	SCROLL_UNBACKED,
	SCROLL_ALIGNED,
};

struct scroll_aligned_backend {
	paddr_t page;
};

struct scroll {
	vaddr_t vaddr;
	vaddr_t size;
	enum scroll_type type;
	struct scroll_aligned_backend aligned_backend;
	struct scroll *next;
};

void scroll_unmap(struct scroll scr);
struct scroll *insert_scroll(struct scroll *scr, struct scroll *first_scr);

#endif
