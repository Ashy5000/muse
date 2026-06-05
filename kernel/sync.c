#include "sync.h"
#include "context.h"
#include "logging.h"
#include <stdbool.h>

/* SIMPLE LOCKS:
 * `struct lock_simple` represents a binary semaphore, which can either be
 * locked or unlocked. That's it! */

void lock_simple_acquire(volatile struct lock_simple *lock) {
	uint32_t expected = 0;
	while (!__atomic_compare_exchange_n(&lock->stat, &expected, LSTAT_WRITE,
	                                    false, __ATOMIC_RELAXED,
	                                    __ATOMIC_RELAXED)) {
	}
}

void lock_simple_release(volatile struct lock_simple *lock) { lock->stat = 0; }

/* REENTRANT LOCKS:
 * `struct lock_reentrant` represents a lock extremely similar to a simple lock.
 * The only difference is that the lock automatically tracks which thread
 * controls it, and allows that thread to hold the lock multiple times
 * simultaneously. The outward-facing interface is identical to a simple lock.
 */

extern struct context *active_ctx;

void lock_reentrant_acquire(volatile struct lock_reentrant *lock) {
	if (lock->owner == active_ctx->id) {
		lock->cnt++;
		return;
	}
	uint32_t expected = 0;
	while (!__atomic_compare_exchange_n(
	    &lock->owner, &expected, active_ctx->id, false, __ATOMIC_RELAXED,
	    __ATOMIC_RELAXED)) {
	}
	lock->cnt = 1;
}

void lock_reentrant_release(volatile struct lock_reentrant *lock) {
	lock->cnt--;
	if (!lock->cnt) {
		lock->owner = 0;
	}
}

/* MULTI-LOCKS:
 * `struct lock_multi` represents a lock which at any point in time can support
 * either a) any number of concurrent readers, or b) a single writer. It also
 * supports two new operations, from simple locks, namely, wait and signal,
 * which allow readers to block until the active writer relenquishes its hold on
 * the lock and simultaneously gives control explicitly to the readers. This is
 * helpful for blocking I/O. */

void lock_multi_acquire_read(volatile struct lock_multi *lock) {
	__atomic_add_fetch(&lock->waiting_readers, 1, __ATOMIC_RELAXED);
	uint32_t expected = 0;
	while (lock->stat != LSTAT_READ &&
	       !__atomic_compare_exchange_n(&lock->stat, &expected, LSTAT_READ,
	                                    false, __ATOMIC_RELAXED,
	                                    __ATOMIC_RELAXED)) {
	}
}

void lock_multi_wait(volatile struct lock_multi *lock) {
	while (lock->stat == LSTAT_READ) {
	}
	__atomic_add_fetch(&lock->waiting_readers, 1, __ATOMIC_RELAXED);
	while (lock->stat != LSTAT_READ) {
	}
}

void lock_multi_release_read(volatile struct lock_multi *lock) {
	if (!__atomic_sub_fetch(&lock->waiting_readers, 1, __ATOMIC_RELAXED)) {
		lock->stat = 0;
	}
}

void lock_multi_acquire_write(volatile struct lock_multi *lock) {
	uint32_t expected = 0;
	while (!__atomic_compare_exchange_n(&lock->stat, &expected, LSTAT_WRITE,
	                                    false, __ATOMIC_RELAXED,
	                                    __ATOMIC_RELAXED)) {
		if (lock->stat == LSTAT_READ) {
			return;
		}
	}
}

void lock_multi_signal(volatile struct lock_multi *lock) {
	if (lock->stat != LSTAT_WRITE) {
		THERE_ARE_FOUR_LIGHTS(
		    "a lock not held by a writer attempted to signal readers");
	}
	if (lock->waiting_readers) {
		lock->stat = LSTAT_READ;
		return;
	}
	lock->stat = 0;
}

void lock_multi_release_write(volatile struct lock_multi *lock) {
	lock->stat = 0;
}
