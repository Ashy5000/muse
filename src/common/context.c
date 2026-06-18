#include <muse/alloc.h>
#include <muse/apic.h>
#include <muse/context.h>
#include <muse/hpet.h>
#include <muse/scheduler.h>

struct context *active_ctx = 0;
struct context *next_ctx   = 0;
struct context *last_ctx   = 0;

struct context first_ctx;

uint32_t ctx_id = 0;

extern void __attribute__((cdecl)) context_switch(struct context *ctx_new);

void create_context(func_ptr_t func_ptr, uint8_t priority, bool user,
                    struct scroll *first_scr, vaddr_t limit,
                    struct vfs_inode *stdin, struct vfs_inode *stdout,
                    struct vfs_inode *stderr) {
	lock_scheduler();
	struct context *ctx_new = kmalloc(sizeof(struct context));
	ctx_new->priority       = priority;
	ctx_new->ctx_heap       = active_ctx->ctx_heap;
	ctx_new->present        = true;
	ctx_new->id             = ctx_id++;
	ctx_new->page_directory =
	    create_task_directory(func_ptr, user, first_scr);
	ctx_new->slices_remaining = 0;
	ctx_new->esp   = (uintptr_t)(TASK_STACK_BASE - (5 * sizeof(uint32_t)));
	ctx_new->limit = limit;
	ctx_new->next  = 0;
	ctx_new->user  = user;
	for (unsigned int i = 0; i < FOPEN_MAX; i++) {
		ctx_new->files[i].mode = 0;
	}
	/* Stdin */
	ctx_new->files[0].mode  = MODE_READ;
	ctx_new->files[0].pos   = 0;
	ctx_new->files[0].inode = stdin;
	/* Stdout */
	ctx_new->files[1].mode  = MODE_WRITE;
	ctx_new->files[1].pos   = 0;
	ctx_new->files[1].inode = stdout;
	/* Stderr */
	ctx_new->files[2].mode  = MODE_WRITE;
	ctx_new->files[2].pos   = 0;
	ctx_new->files[2].inode = stderr;
	if (next_ctx == 0) {
		active_ctx->next = ctx_new;
		next_ctx         = ctx_new;
		last_ctx         = ctx_new;
	} else {
		last_ctx->next = ctx_new;
		last_ctx       = ctx_new;
	}
	unlock_scheduler();
}

void create_kernel_context(func_ptr_t func_ptr, struct scroll *first_scr) {
	struct context *ctx_new = kmalloc(sizeof(struct context));
	ctx_new->ctx_heap       = active_ctx->ctx_heap;
	ctx_new->present        = true;
	ctx_new->id             = ctx_id++;
	ctx_new->page_directory = create_kernel_directory(func_ptr, first_scr);
	ctx_new->slices_remaining = 0;
	ctx_new->esp  = (uintptr_t)(TASK_STACK_BASE - (5 * sizeof(uint32_t)));
	ctx_new->next = 0;
	ctx_new->user = false;
	for (unsigned int i = 0; i < FOPEN_MAX; i++) {
		ctx_new->files[i].mode = 0;
	}
	if (next_ctx == 0) {
		active_ctx->next = ctx_new;
		next_ctx         = ctx_new;
		last_ctx         = ctx_new;
	} else {
		last_ctx->next = ctx_new;
		last_ctx       = ctx_new;
	}
}

void init_first_ctx() {
	active_ctx           = &first_ctx;
	active_ctx->present  = true;
	active_ctx->id       = ctx_id++;
	active_ctx->next     = 0;
	active_ctx->priority = 1;
	__asm__ volatile("mov %%cr3, %0" : "=g"(active_ctx->page_directory));
}
