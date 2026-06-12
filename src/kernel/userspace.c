#include <muse/context.h>
#include <muse/paging.h>
#include <muse/scheduler.h>
#include <muse/sync.h>
#include <muse/userspace.h>

extern struct context *active_ctx;

// TODO: Make data passing between bootloader and kernel better
uint32_t *tss = (uint32_t *)0x7d96;

struct lock_simple runway_lock;
func_ptr_t userspace_runway;
uint32_t argc;
char **argv;

extern void jump_ring3(void);

void load_user_call_info(func_ptr_t func_ptr, uint32_t argc_p, char **argv_p) {
	lock_simple_acquire(&runway_lock);
	userspace_runway = func_ptr;
	argc             = argc_p;
	argv             = argv_p;
}

__attribute__((noreturn)) void enter_ring3() {
	unlock_scheduler();
	tss[1] = TASK_STACK_BASE; // Doesn't matter if this overwrites data-
	                          // this function never returns.
	lock_simple_release(
	    &runway_lock); /* FIXME: There is a possible race condition here. */
	jump_ring3();
	__builtin_unreachable();
}

char *clean_string(unsafe_ptr str_a) {
	char *str    = str_a;
	vaddr_t page = ALIGN_PG_DOWN(str);
	if (!check_user(page)) {
		return 0;
	}
	for (;;) {
		while ((vaddr_t)str < page + PAGE_SIZE) {
			if (!*str) {
				return (char *)str_a;
			}
			str++;
		}
		page += PAGE_SIZE;
		if (!check_user(page)) {
			return 0;
		}
	}
}

void *clean_data(unsafe_ptr data, size_t size) {
	for (uint32_t pg = ALIGN_PG_DOWN(data); pg < ALIGN_PG_UP(data + size);
	     pg += PAGE_SIZE) {
		if (!check_user(pg)) {
			return 0;
		}
	}
	return data;
}

void init_userspace() { runway_lock.stat = 0; }
