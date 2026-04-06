#include "paging.h"

enum scroll_type {
	SCROLL_FAILED, SCROLL_ALIGNED,
};

struct scroll_aligned_backend {
	paddr_t page;
};

struct scroll {
	vaddr_t vaddr;
	vaddr_t size;
	enum scroll_type type;
	struct scroll_aligned_backend aligned_backend;
};
