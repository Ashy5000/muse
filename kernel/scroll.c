#include "scroll.h"
#include "paging.h"

void scroll_unmap(struct scroll scr) {
	if (scr.type == SCROLL_ALIGNED) {
		unmap_page(scr.vaddr);
	}
}
