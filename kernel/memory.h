void kmemcpy(void *dst, void *src, unsigned long bytes);
void *kpage_alloc(void);
void init_memory(void);
void *kmalloc(long size);
void kfree(void *ptr);
