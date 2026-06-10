#include <muse/syscall.h>
#include <muse/alloc.h>
#include <muse/context.h>
#include <muse/logging.h>
#include <muse/paging.h>
#include <muse/userspace.h>
#include <muse/vfs.h>

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
	while (ALIGN_PG_DOWN(active_ctx->limit - 1) <
	       ALIGN_PG_DOWN(prev_limit + inc - 1)) {
		active_ctx->limit += PAGE_SIZE;
		log(LOG_DEBUG, LOG_SYSCALL, "Mapping page %x.\n",
		    ALIGN_PG_DOWN(active_ctx->limit - 1));
		map_page(ALIGN_PG_DOWN(active_ctx->limit - 1),
		         (paddr_t)kpage_alloc());
	}
	while (ALIGN_PG_DOWN(active_ctx->limit - 1) >
	       ALIGN_PG_DOWN(prev_limit + inc - 1)) {
		active_ctx->limit -= PAGE_SIZE;
		kpage_set_status(get_page_mapping(active_ctx->limit), false);
	}

	active_ctx->limit = prev_limit + inc;
	log(LOG_DEBUG, LOG_SYSCALL, "Finished sbrk(%x) -> %x.\n", inc,
	    prev_limit);
	return prev_limit;
}

uint32_t syscall_open(uint32_t *args) {
	char *path = clean_string((unsafe_ptr)(vaddr_t)args[0]);
	log(LOG_DEBUG, LOG_SYSCALL, "Opening file at %s.\n", path);
	uint8_t mode = args[1];
	if (!path) {
		log(LOG_DEBUG, LOG_SYSCALL,
		    "User program supplied corrupt string to open().\n");
		return -1;
	}

	struct vfs_inode *inode = vfs_open(path);
	if (!inode) {
		// TODO: If MODE_CREATE is set, create a file.
		return -1;
	}
	inode->refs++;

	for (unsigned int i = 0; i < FOPEN_MAX; i++) {
		if (active_ctx->files[i].mode == 0) {
			active_ctx->files[i].inode = inode;
			active_ctx->files[i].mode  = mode;
			if (mode & MODE_APPEND) {
				active_ctx->files[i].pos = inode->size;
			} else {
				active_ctx->files[i].pos = 0;
			}
			if (mode & MODE_TRUNCATE) {
				inode->truncate(inode);
			}
			log(LOG_DEBUG, LOG_SYSCALL,
			    "open() finished successfully.\n");
			return i;
		}
	}
	return -1;
}

/* IMPORTANT: transfer() uses the sign of the file descriptor to indicate
 * read/write mode. Positive = read, negative = write. This saves a register,
 * and puts the extra unused bits of the FD (which is always -15 <= 15) to use.
 */
uint32_t syscall_transfer(uint32_t *args) {
	int fd            = args[0];
	enum data_dir dir = DIR_READ;
	if (fd < 0) {
		dir = DIR_WRITE;
		fd  = -fd;
	}
	if (fd >= FOPEN_MAX) {
		return -1;
	}
	size_t size = args[1]; /* TODO: Verify this better. */
	void *data  = clean_data((unsafe_ptr)(vaddr_t)args[2], size);
	if (!data) {
		return -1;
	}

	log(LOG_DEBUG, LOG_SYSCALL, "Transferring %x bytes with transfer().\n",
	    size);

	struct file *f = &active_ctx->files[fd];
	uint32_t bytes_transferred =
	    f->inode->transfer(f->inode, f->pos, size, data, dir);
	f->pos += bytes_transferred;
	return bytes_transferred;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* IMPORTANT: Unlike the C standard library function fseek(), the seek() syscall
 * returns the new offset into the file following the seek. */
uint32_t syscall_seek(uint32_t *args) {
	int fd         = args[0];
	int32_t offset = args[1];
	int whence     = args[2];
	switch (whence) {
	case SEEK_SET:
		active_ctx->files[fd].pos = offset;
		break;
	case SEEK_CUR:
		active_ctx->files[fd].pos += offset;
		break;
	case SEEK_END:
		active_ctx->files[fd].pos =
		    active_ctx->files[fd].inode->size - 1 + offset;
		break;
	}
	return active_ctx->files[fd].pos;
}

uint32_t syscall_close(uint32_t *args) {
	int fd                     = args[0];
	active_ctx->files[fd].mode = 0;
	active_ctx->files[fd].pos  = 0;
	struct vfs_inode *inode    = active_ctx->files[fd].inode;
	inode->refs--;
	if (!inode->refs) {
		kfree(inode->backend_data);
		inode->present = false;
	}
	active_ctx->files[fd].inode = 0;
	return 0;
}

void init_syscalls() {
	syscalls[0] = syscall_exit;
	syscalls[1] = syscall_sbrk;
	syscalls[2] = syscall_open;
	syscalls[3] = syscall_transfer;
	syscalls[4] = syscall_seek;
	syscalls[5] = syscall_close;
}
