#include "paging.h"

struct scroll_backend {
	void (*destroy)(void);
};

struct scroll {
	vaddr_t vaddr;
	uint32_t refs;
	struct scroll_backend backend;
};
