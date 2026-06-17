#include <muse/alloc.h>
#include <muse/logging.h>
#include <muse/term.h>
#include <muse/text.h>
#include <muse/utils.h>

struct vfs_inode *root_term;

uint32_t term_transfer(struct vfs_inode *inode, uint32_t offset_p,
                       uint32_t size_p, void *data, enum data_dir dir) {
	__asm__ volatile(
	    "sti"); /* Allow context switching during this process */
	if (dir == DIR_WRITE) {
		log(LOG_DEBUG, LOG_TERM, "Writing %x bytes to term.\n", size_p);
		for (uint32_t i = 0; i < size_p; i++) {
			console_put_char(((char *)data)[i], 0xFFFFFF);
		}
	} else {
		volatile struct stdin *in = inode->backend_data;
		uint32_t offset           = offset_p;
		uint32_t size             = size_p;

		log(LOG_DEBUG, LOG_TERM,
		    "Reading %x bytes from stdin at offset %x.\n", size,
		    offset);

		lock_multi_acquire_read(in->lock);

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
			if (size) {
				if (in->active_line + 1 == TERM_BFR_HEIGHT) {
					/* If the buffer is going to scroll, the
					 * index of the line we wanted to read
					 * will decrease by 1. */
					line_idx--;
				}
				lock_multi_release_read(in->lock);
				lock_multi_wait(in->lock);
			} else {
				lock_multi_release_read(in->lock);
			}
		}
		log(LOG_DEBUG, LOG_TERM, "Terminal stdin read done.\n");
	}
	return size_p;
}

void term_stdin_putc(struct vfs_inode *inode, char c) {
	struct stdin *in = inode->backend_data;
	lock_multi_acquire_write(in->lock);
	/* FIXME: Put locks on writing to stdin. */
	in->lines[in->active_line].s[in->lines[in->active_line].len++] = c;
	if ((in->lines[in->active_line].len >= TERM_BFR_WIDTH || c == '\n')) {
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
		lock_multi_signal(in->lock);
	} else {
		lock_multi_release_write(in->lock);
	}
}

void init_root_term() {
	struct vfs_inode inode;
	inode.size         = 0;
	inode.present      = true;
	inode.first_child  = 0;
	inode.backend_data = 0;
	inode.transfer     = term_transfer;
	struct stdin *in   = kmalloc(sizeof(*in));
	in->active_line    = 0;
	in->bfr_offset     = 0;
	in->lock           = kmalloc(sizeof(*in->lock));
	for (size_t i = 0; i < TERM_BFR_HEIGHT; i++) {
		in->lines[i].len = 0;
	}
	inode.backend_data = in;
	struct vfs_mount_point *mnt =
	    mount(inode, "/dev/term"); /* TODO: Dynamically generate terminal
	                                  mount points. */
	root_term = &mnt->inode;
	log(LOG_INFO, LOG_TERM, "Root terminal device created.\n");
}
