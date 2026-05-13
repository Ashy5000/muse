#include "scroll.h"
#include "paging.h"
#include "alloc.h"

void scroll_unmap(struct scroll scr) {
	if (scr.type == SCROLL_ALIGNED) {
		kfree((void*)(uintptr_t)scr.vaddr);
		unmap_page(scr.aligned_backend.page);
	}
}
