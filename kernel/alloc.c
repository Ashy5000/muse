#include "alloc.h"
#include "paging.h"
#include "context.h"
#include "scroll.h"
#include "../drivers/text.h"

void fuse_blocks(struct block_header *header) {
	struct block_header *header_next = (void*)header + sizeof(struct block_header) + header->size;
	if (header_next->free == 1 || header_next->free == 3) {
		header->size += sizeof(struct block_header) + header_next->size;
	}
	header->free = 1;
}

void *kmalloc(vaddr_t size, struct context ctx) {
	void *block = ctx.heap;
	struct block_header *header;
	while(1) {
		header = block;
		if (header->free == 2) {
			return 0;
		}
		if (header->free == 1) {
			fuse_blocks(header);
		}
		if (header->size >= size && header->free) {
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
				header->size = size;
			}
			header->free = 0;
			vaddr_t begin = (uintptr_t)header + sizeof(struct block_header);
			return (void*)(uintptr_t)begin;
		} else {
			block += header->size + sizeof(struct block_header);
		}
	}
}

struct scroll kmalloc_page(struct context ctx) {
	struct block_header *header = ctx.heap;
	while(1) {
		if (header->free == 2) {
			struct scroll res;
			res.vaddr = 0;
			res.size = 0;
			res.type = SCROLL_FAILED;
			return res;
		}
		if (header->free == 1) {
			fuse_blocks(header);
		}
		vaddr_t page_start = (uintptr_t)header + PAGE_SIZE - ((uintptr_t)header % PAGE_SIZE);
		vaddr_t page_end = page_start + PAGE_SIZE;
		vaddr_t header_start = page_start - sizeof(struct block_header);
		while (header_start < (uintptr_t)header + sizeof(struct block_header)) {
			page_start += PAGE_SIZE;
			page_end += PAGE_SIZE;
			header_start += PAGE_SIZE;
		}
		paddr_t page;
		__asm__ volatile ("xchgw %bx, %bx");
		if (header->size >= PAGE_SIZE && header->free) {
			vaddr_t header_page_start = header_start - (header_start % PAGE_SIZE);
			if (!get_page_mapping(header_page_start)) {
				map_page(header_page_start, (uintptr_t)kpage_alloc());
			}
			page = get_page_mapping(page_start);
			if (!page) {
				page = (uintptr_t)kpage_alloc();
				map_page(page_start, page);
			}
			struct block_header *page_header = (struct block_header*)(uintptr_t)header_start;
			page_header->free = 0;
			page_header->size = (uintptr_t)header + header->size - page_start;
			struct block_header *excess_header = (struct block_header*)(uintptr_t)(page_end);
			vaddr_t excess_page = (uintptr_t)excess_header - ((uintptr_t)excess_header % PAGE_SIZE);
			if ((uintptr_t)(excess_header + 1) < (uintptr_t)header + header->size) {
				if (!get_page_mapping(excess_page)) {
					map_page(excess_page, (uintptr_t)kpage_alloc());
				}
				excess_header->size = (uintptr_t)header + header->size - (uintptr_t)excess_header;
				excess_header->free = 1;
				page_header->size = PAGE_SIZE;
			}
			header->size = header_start - (uintptr_t)header - sizeof(struct block_header);
			struct scroll res;
			res.vaddr = page_start;
			res.size = PAGE_SIZE;
			res.type = SCROLL_ALIGNED;
			res.aligned_backend.page = page;
			return res;
		} else {
			header = (struct block_header*)((uintptr_t)header + header->size + sizeof(struct block_header));
		}
	}
}

void kfree(void *p) {
	struct block_header *header = (struct block_header*)p - 1;
	header->free = 1;
}
