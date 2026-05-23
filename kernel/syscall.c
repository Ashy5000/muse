#include "syscall.h"
#include "context.h"
#include "logging.h"
#include "paging.h"

extern struct context *active_ctx;

syscall_t syscalls[16];

void handle_syscall(uint32_t *args) {
	uint32_t fn = args[0];
	if (fn > 15) {
		return;
	}
	if (syscalls[fn]) {
		syscalls[fn](args + 1);
	}
}

__attribute__((noreturn)) uint32_t syscall_exit(__attribute__((unused))
                                                uint32_t *args) {
	log(LOG_DEBUG, LOG_SYSCALL, "exit() called with code %i\n", args[0]);
	terminate();
	__builtin_unreachable();
}

// TODO: Validate input for sbrk()
uint32_t syscall_sbrk(uint32_t *args) {
	intptr_t inc       = args[0];
	vaddr_t prev_limit = active_ctx->limit;

	// Allocate/free pages as necessary
	while (ALIGN_PG_DOWN(active_ctx->limit - 1) < ALIGN_PG_DOWN(prev_limit + inc - 1)) {
		active_ctx->limit += PAGE_SIZE;
		log(LOG_DEBUG, LOG_SYSCALL, "Mapping page %x.\n", ALIGN_PG_DOWN(active_ctx->limit - 1));
		map_page(ALIGN_PG_DOWN(active_ctx->limit - 1), (paddr_t)kpage_alloc());
	}
	while (ALIGN_PG_DOWN(active_ctx->limit - 1) > ALIGN_PG_DOWN(prev_limit + inc - 1)) {
		active_ctx->limit -= PAGE_SIZE;
		kpage_set_status(get_page_mapping(active_ctx->limit), false);
	}

	active_ctx->limit = prev_limit + inc;
	log(LOG_DEBUG, LOG_SYSCALL, "Finished sbrk(%x) -> %x.\n", inc, prev_limit);
	return prev_limit;
}

void init_syscalls() {
	syscalls[0] = syscall_exit;
	syscalls[1] = syscall_sbrk;
}
