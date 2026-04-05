#include "context.h"
#include "paging.h"
#include "../drivers/text.h"

extern void get_instruction_pointer(void);

uint32_t active_context = 0;
uint32_t contexts_count = 1;
struct context contexts[32];

const mem_t userspace_stack_base = 0x200000;
const mem_t userspace_stack_size = PAGE_SIZE * 16;

// struct context create_user_context(func_ptr_t func_ptr) {
// 	struct context ctx;
// 	uint32_t *page_directory = kpage_alloc();
// 	uint32_t *page_directory_virt = vmalloc_page(0, contexts[active_context]);
// 	map_page((uintptr_t)page_directory_virt, (uintptr_t)page_directory, 0);
// 	for (uint32_t i = 0; i < PAGE_SIZE / 4; i++) {
// 		page_directory_virt[i] = 6;
// 	}
// 	ctx.page_directory = page_directory;
// 	for (vaddr_t i = userspace_stack_base - userspace_stack_size; i < userspace_stack_base; i += PAGE_SIZE) {
// 		kprint(".");
// 		map_page_inactive(i, (uintptr_t)kpage_alloc(), 0, page_directory_virt, true);
// 	}
// 	kprint("Done!\n");
	// *((func_ptr_t*)(uintptr_t)userspace_stack_base) = func_ptr;
	// ctx.esp = userspace_stack_base + sizeof(func_ptr_t);
	// ctx.heap = (void*)(uintptr_t)userspace_stack_base + 1;
// 	return ctx;
// }

// void context_switch(struct context new_context) {
// 	// TODO: Store esp
// 	// Push registers onto the old stack
// 	__asm__ volatile ("pushal");
// 	// Push instruction pointer onto the old stack
// 	__asm__ volatile ("call get_instruction_pointer");
// 	__asm__ volatile (".set switch_begin, .");
// 	__asm__ volatile ("add %ebx, switch_end - switch_begin");
// 	__asm__ volatile ("push %ebx");
// 	// Load new paging structures
// 	__asm__ volatile ("mov %0, %%cr3" :: "r"(new_context.page_directory) : "memory" );
// 	// Switch stack base pointer
// 	__asm__ volatile ("mov %0, %%ebp" :: "r"(new_context.ebp) : "memory" );
// 	// Switch stack pointer
// 	__asm__ volatile ("mov %0, %%esp" :: "r"(new_context.esp) : "memory" );
// 	// Pop instruction pointer off of the stack
// 	__asm__ volatile ("pop %ebx");
// 	// Pop registers off of new stack
// 	__asm__ volatile ("popal");
//
// 	// MAKE THE SWITCH!!!
// 	__asm__ volatile ("jmp %ebx");
// 	__asm__ volatile (".set switch_end, .");
// }
