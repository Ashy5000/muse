#include "memory.h"
#include "../drivers/text.h"

int main() {
	init_memory();
	char *data = "Hello, world!";
	char *str = kmalloc(13);
	kmemcpy(str, data, 13);
	kprint(str);

	while (1) {}
}
