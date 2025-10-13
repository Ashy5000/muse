#include "memory.h"
#include "../drivers/text.h"

void kmemcpy(void *dst, void *src, unsigned long bytes) {
	for (int i = 0; i < bytes; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
}

struct block_header {
	int size;
	int free; // 0 = in use, 1 = free, 2 = end of memory
};

void *MEM_START;

void *mmap_table = (void*)0x0504;

void init_memory(void) {
	int entry_count = *(int*)0x0500;
	for (int i = 0; i < entry_count; i++) {
		if (*((int*)(mmap_table + sizeof(int) * 6 * i + 4 * sizeof(int))) == 1) {
			struct block_header *header = *((struct block_header**)(mmap_table + sizeof(int) * 6 * i));
			header->size = *((int*)(mmap_table + sizeof(int) * 6 * i + sizeof(int) * 2)) - sizeof(struct block_header);
			header->free = 1;
			struct block_header *header_last = header + header->size;
			header_last->size = 0;
			header_last->free = 2;
			MEM_START = header;
		}
	}
}

void fuse_blocks(struct block_header *header) {
	struct block_header *header_next = (void*)header + sizeof(struct block_header) + header->size;
	if (header_next->free == 1 || header_next->free == 3) {
		header->size += sizeof(struct block_header) + header_next->size;
	}
	header->free = 1;
}

void *kmalloc(long size) {
	void *block = MEM_START;
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
			if (header->size - size > sizeof(struct block_header)) {
				struct block_header *new_header = block + size + sizeof(struct block_header);
				new_header->size = header->size - size - sizeof(struct block_header);
				new_header->free = 1;
			}
			header->size = size;
			header->free = 0;
			return (void*)header + sizeof(struct block_header);
		} else {
			block += header->size + sizeof(struct block_header);
		}
	}
}

void kfree(void *ptr) {
	struct block_header *header = ptr - sizeof(struct block_header);
	header->free = 1;
}
