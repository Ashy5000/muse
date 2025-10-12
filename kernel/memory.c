#include "memory.h"

struct block_header {
	void *data;
	long size;
	unsigned int free;
};

void *MEM_START = (void*)0x100000;

struct block_header *get_header(void *block) {
	return ((struct block_header*) block);
}

void init_memory(void) {
	struct block_header *first_header = get_header(MEM_START);
	first_header->data = MEM_START + sizeof(struct block_header);
	first_header->size = 100;
	first_header->free = 1;
}

void *kmalloc(long size) {
	void *block = MEM_START;
	struct block_header *header;
	while(1) {
		header = get_header(block);
		if (header->size >= size && header->free) {
			if (header->size - size > sizeof(struct block_header)) {
				struct block_header *new_header = get_header(block + size);
				new_header->size = header->size - size - sizeof(struct block_header);
				new_header->free = 1;
				new_header->data = header->data + header->size - new_header->size;
			}
			header->size = size;
			header->free = 0;
			return header->data;
		} else {
			block += header->size + sizeof(struct block_header);
		}
	}
}

void kmemcpy(void *dst, void *src, unsigned long bytes) {
	for (int i = 0; i < bytes; i++) {
		((char*)dst)[i] = ((char*)src)[i];
	}
}
