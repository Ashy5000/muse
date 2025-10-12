#include "memory.h"
#include "../drivers/text.h"

int main() {
	init_memory();
	char *a = kmalloc(13);
	kprint_int((int)(a - 0x100000));
	char *b = kmalloc(2);
	kprint_int((int)(b - 0x100000));
	kfree(b);
	kfree(a);
	char *c = kmalloc(14);
	kprint_int((int)(c - 0x100000));

	while (1) {}
}
