#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#define TIER_IDX(X) ((31 - __builtin_clz((uintptr_t)X)) - MIN_CHUNK_SIZE_LOG)
#define MIN_CHUNK_SIZE_LOG 4
#define MIN_CHUNK_SIZE     (1 << MIN_CHUNK_SIZE_LOG)
#define MAX_CHUNK_SIZE_LOG 15
#define MAX_CHUNK_SIZE     (1 << MAX_CHUNK_SIZE_LOG)
#define TIER_CNT           MAX_CHUNK_SIZE_LOG - MIN_CHUNK_SIZE_LOG + 1
#define MERGE_THRESH       2
#define CHUNK_OVERHEAD     (2 * sizeof(size_t))
#define BIT_FREE           1
#define SIZE_MASK          (~BIT_FREE)

struct heap {
	size_t size;
	size_t free;
	struct chunk *tiers[TIER_CNT];
	void *limit;
};

struct chunk {
	size_t prev_size;
	size_t size; // Includes overhead
	struct chunk *next;
	// The end of the chunk contains an identical copy of size.
	// This makes combining chunks faster.
};

struct heap global_heap;

void init_heap() {
	global_heap.size = 0;
	global_heap.free = 0;
	for (unsigned int i = 0; i < TIER_CNT; i++) {
		global_heap.tiers[i] = 0;
	}
	// There is always a prev_size stored at the end of the heap
	size_t *root_prev_size = sbrk(sizeof(*root_prev_size));
	global_heap.limit = (void *)root_prev_size + sizeof(*root_prev_size);
	*root_prev_size   = 0;
}

void split_chunk(struct chunk *ch, size_t size) {
	/* These chunks are not next to each other in the linked list: they are
	 * adjacent in memory. */
	struct chunk *next_ch =
	    (void *)ch +
	    (ch->size & SIZE_MASK); /* The chunk after the one being split. */
	struct chunk *new_ch =
	    (void *)ch + size +
	    CHUNK_OVERHEAD; /* The new chunk being created in the split. */

	/* Calculate the size of the new chunk. This *includes* its
	 * overhead. */
	new_ch->size =
	    ((ch->size & SIZE_MASK) - size - CHUNK_OVERHEAD) | BIT_FREE;
	ch->size           = size + CHUNK_OVERHEAD;
	new_ch->prev_size  = ch->size;

	next_ch->prev_size = new_ch->size; /* Because we are changing the size
	                                      of chunks, we need to inform the
	                                      next one of our changes. */

	/* Put the new chunk into the correct tier. */
	unsigned int idx   = TIER_IDX(new_ch);
	new_ch->next       = global_heap.tiers[idx];
	/* TODO: Don't completely obliterate the cache. */
	global_heap.tiers[idx] = new_ch;
}

void *malloc(size_t size) {
	unsigned int i =
	    TIER_IDX(size); /* The index of the tier the chunk will be in. */
	struct chunk *ch =
	    global_heap.tiers[i]; /* The chunk we are examining. */
	struct chunk *prev = 0;   /* The previous chunk. */
	while (ch) {
		if ((ch->size & SIZE_MASK) < size + CHUNK_OVERHEAD) {
			/* There isn't enough room in the chunk. */
			prev = ch;
			ch   = ch->next;
			continue;
		}
		/* Remove from linked list */
		if (prev) {
			prev->next = ch->next;
		} else {
			global_heap.tiers[i] = ch->next;
		}

		/* Is it worth it to split into two chunks?
		   TODO: Parametrize this better */
		if ((ch->size & ~BIT_FREE) >=
		    size + MIN_CHUNK_SIZE + (2 * CHUNK_OVERHEAD)) {
			split_chunk(ch, size);
		}
		return (void *)ch + CHUNK_OVERHEAD;
	}

	struct chunk *new_ch =
	    global_heap.limit - sizeof(size_t); /* Overlap with prev_size stored
	                                           at the end of the heap */
	sbrk(size + CHUNK_OVERHEAD); /* Make sure to allocate a new terminating
	                                       prev_size */
	new_ch->size = size + CHUNK_OVERHEAD;
	global_heap.limit += new_ch->size;
	return (void *)new_ch + CHUNK_OVERHEAD;
}
