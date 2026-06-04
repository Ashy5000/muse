#include "term.h"
#include "../drivers/text.h"
#include "alloc.h"
#include "logging.h"
#include "utils.h"

struct vfs_inode *root_term;

void stdin_reader_wait(volatile struct stdin *in) {
	while (in->transaction_active) { /* Wait until the last
		                            transaction is done */
	}
	__atomic_add_fetch(&in->waiting_readers, 1,
	                   __ATOMIC_RELAXED); /* We don't care what order the
	                                         increments happen, only that
	                                         they all *do* happen. This
	                                         atomic operation enforces
	                                         this. */
	while (!in->transaction_active) {     /* Wait until new
		                                 transaction starts.
		                                 (We can only read
		                                 data during a
		                                 transaction.) */
	}
}

void stdin_reader_lock(volatile struct stdin *in) {
	__atomic_add_fetch(&in->waiting_readers, 1, __ATOMIC_RELAXED);
	in->transaction_active = true;
}

void stdin_reader_unlock(volatile struct stdin *in) {
	__atomic_sub_fetch(&in->waiting_readers, 1,
	                   __ATOMIC_RELAXED); /* Let the writer know that we
	                                         are done. */
	if (in->waiting_readers == 0) {
		/* This assignment doesn't have to be atomic,
		 * because it is setting the field to a constant
		 * value. */
		in->transaction_active = false;
	}
}

uint32_t term_transfer(struct vfs_inode *inode, uint32_t offset_p,
                       uint32_t size_p, void *data, enum hal_drive_dir dir) {
	__asm__ volatile(
	    "sti"); /* Allow context switching during this process */
	if (dir == DRV_WRITE) {
		for (uint32_t i = 0; i < size_p; i++) {
			kput_char(((char *)data)[i]);
		}
	} else {
		volatile struct stdin *in = inode->backend_data;
		uint32_t offset           = offset_p;
		uint32_t size             = size_p;

		log(LOG_DEBUG, LOG_TERM,
		    "Reading %x bytes from stdin at offset %x.\n", size,
		    offset);

		stdin_reader_lock(in);

		/* TODO: Store line offset instead of size, so
		 * we can use binary search */
		uint32_t line_offset = in->bfr_offset;
		size_t line_idx      = 0;
		while (1) {
			if (line_offset + in->lines[line_idx].len > offset) {
				break;
			}
			line_offset += in->lines[line_idx].len;
			if (line_idx >= in->active_line) {
				break;
			}
			line_idx++;
		}
		uint32_t char_offset = offset - line_offset;
		if (offset <= in->bfr_offset) {
			offset      = in->bfr_offset;
			line_idx    = 0;
			char_offset = 0;
		}

		while (size) { /* This might take multiple transactions if size
			          is sufficiently large, or none at all if the
			          data is already present */
			while (size && line_idx <= in->active_line) {
				uint32_t transfer_size =
				    MIN(size,
				        in->lines[line_idx].len - char_offset);
				if (line_idx == in->active_line &&
				    transfer_size < size) {
					/* We should wait to read the entire
					 * line, as it isn't guarenteed to be
					 * finished yet. */
					break;
				}
				log(LOG_DEBUG, LOG_TERM,
				    "Copying %x bytes from line %x, char %x.\n",
				    transfer_size, line_idx, char_offset);
				memcpy(data,
				       (void *)in->lines[line_idx].s +
				           char_offset,
				       transfer_size);
				line_idx++;
				char_offset = 0;
				data += transfer_size;
				size -= transfer_size;
			}
			stdin_reader_unlock(in);
			if (size) {
				if (in->active_line + 1 == TERM_BFR_HEIGHT) {
					/* If the buffer is going to scroll, the
					 * index of the line we wanted to read
					 * will decrease by 1. */
					line_idx--;
				}
				stdin_reader_wait(in);
			}
		}
		log(LOG_DEBUG, LOG_TERM, "Terminal stdin read done.\n");
	}
	return size_p;
}

void term_stdin_putc(struct vfs_inode *inode, char c) {
	struct stdin *in = inode->backend_data;
	if (in->transaction_active) {
		/* TODO: Don't drop keypresses */
		return;
	}
	/* FIXME: Put locks on writing to stdin. */
	in->lines[in->active_line].s[in->lines[in->active_line].len++] = c;
	if ((in->lines[in->active_line].len >= TERM_BFR_WIDTH || c == '\n') &&
	    in->waiting_readers) { /* If there aren't any waiting readers,
		                      a) this is a waste, and
		                      b) there won't be anyone to close the
		                      transaction, blocking further readers. */
		in->active_line++;
		if (in->active_line == TERM_BFR_HEIGHT) {
			/* TODO: Maybe do this more than one line at a time to
			 * save on memcpy calls? */
			in->bfr_offset += in->lines[0].len;
			memcpy(in->lines, in->lines + 1,
			       (TERM_BFR_HEIGHT - 1) *
			           sizeof(struct stdin_line));
			in->active_line--;
			in->lines[in->active_line].len = 0;
		}
		in->transaction_active = true; /* Start a new transaction */

		/* From here:
		 * 1. Readers that were waiting unblock.
		 * 2. Readers read data.
		 * 3. Readers un-wait themselves (atomically) and the last
		 * one(s) to do so terminate the transaction
		 * 4. New readers start waiting & block */
	}
}

void init_root_term() {
	struct vfs_inode inode;
	inode.size             = 0;
	inode.present          = true;
	inode.first_child      = 0;
	inode.backend_data     = 0;
	inode.transfer         = term_transfer;
	struct stdin *in       = kmalloc(sizeof(*in));
	in->active_line        = 0;
	in->bfr_offset         = 0;
	in->transaction_active = 0;
	in->waiting_readers    = 0;
	for (size_t i = 0; i < TERM_BFR_HEIGHT; i++) {
		in->lines[i].len = 0;
	}
	inode.backend_data = in;
	struct vfs_mount_point *mnt =
	    mount(inode, "/dev/term"); /* TODO: Dynamically generate terminal
	                                  mount points. */
	root_term = &mnt->inode;
}
