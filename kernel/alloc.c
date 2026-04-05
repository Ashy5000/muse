#include "alloc.h"
#include "paging.h"
#include "context.h"

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
				if (!check_page_status(vaddr)) {
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

void *kmalloc_page(struct context ctx) {
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
		vaddr_t page_start = (uintptr_t)block + PAGE_SIZE - ((uintptr_t)block % PAGE_SIZE);
		vaddr_t page_end = page_start + PAGE_SIZE;
		vaddr_t header_start = page_start - sizeof(struct block_header);
		while (header_start < (uintptr_t)block + sizeof(struct block_header)) {
			page_start += PAGE_SIZE;
			page_end += PAGE_SIZE;
			header_start += PAGE_SIZE;
		}
		if (page_end <= (uintptr_t)block + sizeof(struct block_header) + header->size && header->free) {
			vaddr_t header_page_start = header_start - (header_start % PAGE_SIZE);
			if (!check_page_status(header_page_start)) {
				map_page(header_page_start, (uintptr_t)kpage_alloc());
			}
			if (!check_page_status(page_start)) {
				map_page(page_start, (uintptr_t)kpage_alloc());
			}
			struct block_header *page_header = (struct block_header*)(uintptr_t)header_start;
			page_header->free = 0;
			page_header->size = (uintptr_t)block + header->size - page_start;
			header->size = header_start - (uintptr_t)block - sizeof(struct block_header);
			struct block_header *excess_header = (struct block_header*)((uintptr_t)page_header + sizeof(struct block_header) + PAGE_SIZE);
			vaddr_t excess_page = (uintptr_t)excess_header - ((uintptr_t)excess_header % PAGE_SIZE);
			if ((uintptr_t)(excess_header + 1) < (uintptr_t)header + header->size) {
				if (!check_page_status(excess_page)) {
					map_page(excess_page, (uintptr_t)kpage_alloc());
				}
				excess_header->size = (uintptr_t)header + header->size - (uintptr_t)excess_header;
				excess_header->free = 1;
				page_header->size = PAGE_SIZE;
			}
			return (void*)(uintptr_t)page_start;
		} else {
			block += header->size + sizeof(struct block_header);
		}
	}
}

void kfree(void *p) {
	struct block_header *header = (struct block_header*)p - 1;
	header->free = 1;
}
