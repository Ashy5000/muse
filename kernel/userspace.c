#include "userspace.h"
#include "context.h"
#include "paging.h"

extern struct context *active_ctx;

// TODO: Make data passing between bootloader and kernel better
uint32_t *tss = (uint32_t *)0x7d96;

func_ptr_t userspace_runway;
uint32_t argc;
char **argv;

extern void jump_ring3(void);

void load_user_call_info(func_ptr_t func_ptr, uint32_t argc_p, char **argv_p) {
	userspace_runway = func_ptr;
	argc             = argc_p;
	argv             = argv_p;
}

__attribute__((noreturn)) void enter_ring3() {
	unlock_scheduler();
	tss[1] = TASK_STACK_BASE; // Doesn't matter if this overwrites data-
	                          // this function never returns.
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
