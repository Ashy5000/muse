#include "scroll.h"
#include "paging.h"
#include "alloc.h"

void scroll_unmap(struct scroll scr) {
	if (scr.type == SCROLL_ALIGNED) {
		kfree((void*)(uintptr_t)scr.vaddr);
		unmap_page(scr.aligned_backend.page);
	}
}

struct scroll *insert_scroll(struct scroll *scr, struct scroll *first_scr) {
	if (!first_scr) {
		return scr;
	}
	uint32_t scr_start = scr->vaddr;
	uint32_t scr_end = scr_start + scr->size;
	struct scroll *prev_scr = 0;
	struct scroll *current_scr = first_scr;
	while (current_scr) {
		uint32_t current_scr_start = current_scr->vaddr;
		uint32_t current_scr_end = current_scr_start + current_scr->size;
		if (scr_start == current_scr_start && scr_end == current_scr_end) {
			if (prev_scr) {
				prev_scr->next = current_scr->next;
				kfree(current_scr);
			} else {
				first_scr = first_scr->next;
				kfree(current_scr);
			}
		}
		if (scr_start > current_scr_start && scr_end < current_scr_end) {
			struct scroll *new_scr = kmalloc(sizeof(*new_scr));
			new_scr->vaddr = scr_end;
			new_scr->size = current_scr_end - new_scr->vaddr;
			new_scr->type = current_scr->type;
			if (current_scr->type == SCROLL_ALIGNED) {
				new_scr->aligned_backend.page = current_scr->aligned_backend.page + new_scr->vaddr - current_scr_start;
			}
			new_scr->next = first_scr->next;
			first_scr->next = new_scr;
			current_scr->size = scr_start - current_scr_start;
			continue;
		}
		if (scr_start > current_scr_start && scr_start < current_scr_end) {
			current_scr->size = scr_start - current_scr_start;
		}
		if (scr_end > current_scr_start && scr_end < current_scr_end) {
			current_scr->vaddr = scr_end;
			current_scr->size -= scr_end - current_scr_start;
		}
		prev_scr = current_scr;
		current_scr = current_scr->next;
	}
	scr->next = first_scr;
	return scr;
}
