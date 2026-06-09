#include "multiboot.h"

struct multiboot_tag *multiboot_find_tag(void *multiboot, uint32_t type) {
	struct multiboot_tag *tag = (void *)multiboot + 8;
	while ((void *)tag < (void *)multiboot + *((uint32_t *)multiboot)) {
		if (tag->type == type) {
			return tag;
		}
		tag = (void *)tag + tag->size;
		if ((uintptr_t)tag % 8 > 0) {
			tag = (void *)tag + 8 - ((uintptr_t)tag % 8);
		}
	}
	return 0;
}
