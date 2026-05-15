#include "alloc.h"
#include "paging.h"
#include "context.h"
#include "scroll.h"
#include "../drivers/text.h"

extern struct context *active_ctx;

void fuse_blocks(struct block_header *header) {
	if ((header->free & 2) > 0) { // This is the last header, nothing to fuse with.
		return;
	}
	struct block_header *header_next = (struct block_header*)((uintptr_t)header + sizeof(struct block_header) + header->size);
	if (!get_page_mapping((uintptr_t)header_next)) {
		header->size = ((uintptr_t)header_next + PAGE_SIZE - ((uintptr_t)header_next % PAGE_SIZE)) - (uintptr_t)header;
	} else if ((header_next->free & 1) > 0) {
		header->size += sizeof(struct block_header) + header_next->size;
	}
	header->free = 1;
}

void *kmalloc(vaddr_t size) {
	void *block = active_ctx->heap;
	struct block_header *header;
	while(1) {
		header = block;
		if ((header->free & 1) == 0) {
			if ((header->free & 2) > 0) {
				break;
			}
			block += header->size + sizeof(struct block_header);
			continue;
		}
		if (header->size >= size) {
			for (vaddr_t vaddr = (uintptr_t)header & ADDR_MASK; vaddr < (uintptr_t)header + header->size; vaddr += PAGE_SIZE) {
				if (!get_page_mapping(vaddr)) {
					map_page(vaddr, (uintptr_t)kpage_alloc());
				}
			}
			if (header->size - size > sizeof(struct block_header)) {
				struct block_header *new_header = block + size + sizeof(struct block_header);
				vaddr_t page_start = (uintptr_t)new_header - ((uintptr_t)new_header % PAGE_SIZE);
				new_header->size = header->size - size - sizeof(struct block_header);
				new_header->free = 1;
				if ((header->free & 2) > 0) {
					new_header->free = 3;
					header->free &= ~2;
				}
				header->size = size;
			}
			header->free &= ~1;
			vaddr_t begin = (uintptr_t)header + sizeof(struct block_header);
			return (void*)(uintptr_t)begin;
		}
		uint32_t new_block = (uintptr_t)block + header->size + sizeof(struct block_header);
		if (get_page_mapping(new_block)) {
			// Next header exists, everything is fine
			block = (void*)(uintptr_t)new_block;
		} else {
			// Next header doesn't exist. Expand the current header to contain it and try to claim this header again.
			header->size = (new_block + PAGE_SIZE - (new_block % PAGE_SIZE)) - (uintptr_t)block - sizeof(struct block_header);
		}
	}
	kprint("kmalloc() failed: out of memory!\n");
	return 0;
}

void *kmalloc_aligned() {
	struct block_header *header = active_ctx->heap;
	while(1) {
		if ((header->free & 1) == 0) {
			if ((header->free & 2) > 0) {
				break;
			}
			header = (struct block_header*)((uintptr_t)header + header->size + sizeof(struct block_header));
			continue;
		}
		vaddr_t page_start = (uintptr_t)header + PAGE_SIZE - ((uintptr_t)header % PAGE_SIZE);
		vaddr_t page_end = page_start + PAGE_SIZE;
		vaddr_t header_start = page_start - sizeof(struct block_header);
		while (header_start < (uintptr_t)header + sizeof(struct block_header)) {
			page_start += PAGE_SIZE;
			page_end += PAGE_SIZE;
			header_start += PAGE_SIZE;
		}
		if (header->size >= PAGE_SIZE) {
			vaddr_t header_page_start = header_start - (header_start % PAGE_SIZE);
			if (!get_page_mapping(header_page_start)) {
				map_page(header_page_start, (uintptr_t)kpage_alloc());
			}
			struct block_header *page_header = (struct block_header*)(uintptr_t)header_start;
			page_header->free = 0;
			if ((header->free & 2) > 0) {
				page_header->free = 2;
				header->free = 1;
			}
			page_header->size = PAGE_SIZE;
			struct block_header *excess_header = (struct block_header*)(uintptr_t)(page_end);
			vaddr_t excess_page = (uintptr_t)excess_header - ((uintptr_t)excess_header % PAGE_SIZE);
			if ((uintptr_t)(excess_header + 1) < (uintptr_t)header + header->size) {
				if (!get_page_mapping(excess_page)) {
					map_page(excess_page, (uintptr_t)kpage_alloc());
				}
				excess_header->size = (uintptr_t)header + header->size - (uintptr_t)excess_header;
				excess_header->free = 1;
				if ((page_header->free & 2) > 0) {
					excess_header->free = 3;
					page_header->free = 0;
				}
				page_header->size = PAGE_SIZE;
			}
			header->size = header_start - (uintptr_t)header - sizeof(struct block_header);
			return (void*)(uintptr_t)page_start;
		}
		void *new_header = (void*)((uintptr_t)header + header->size + sizeof(struct block_header));
		if (get_page_mapping((uintptr_t)new_header)) {
			header = new_header;
		} else {
			header->size = (uintptr_t)((new_header + PAGE_SIZE - ((uintptr_t)new_header % PAGE_SIZE)) - (uintptr_t)header - sizeof(struct block_header));
		}
	}
	kprint("kmalloc_aligned() failed: out of memory!\n");
	return 0;
}

struct scroll kmalloc_page() {
	uint32_t page_addr = (uintptr_t)kmalloc_aligned();
	struct scroll scr;
	scr.size = 0;
	scr.type = SCROLL_FAILED;
	scr.aligned_backend.page = 0;
	scr.vaddr = 0;
	if (!page_addr) {
		return scr;
	}
	scr.size = PAGE_SIZE;
	scr.type = SCROLL_ALIGNED;
	scr.aligned_backend.page = get_page_mapping(page_addr);
	if (!scr.aligned_backend.page) {
		scr.aligned_backend.page = (uintptr_t)kpage_alloc();
		map_page(page_addr, scr.aligned_backend.page);
	}
	scr.vaddr = page_addr;
	map_page(scr.vaddr, scr.aligned_backend.page);
	return scr;
}

void kfree(void *p) {
	struct block_header *header = (struct block_header*)p - 1;
	header->free |= 1;
	fuse_blocks(header);
}
