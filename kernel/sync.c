#include "sync.h"
#include "logging.h"
#include <stdbool.h>

void lock_multi_acquire_read(volatile struct lock_multi *lock) {
	__atomic_add_fetch(&lock->waiting_readers, 1, __ATOMIC_RELAXED);
	uint32_t expected = 0;
	while (!__atomic_compare_exchange_n(&lock->stat, &expected, LSTAT_READ,
	                                    false, __ATOMIC_RELAXED,
	                                    __ATOMIC_RELAXED)) {
		if (lock->stat == LSTAT_READ) {
			return;
		}
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
		;
		return;
	}
	lock->stat = 0;
}

void lock_multi_release_write(volatile struct lock_multi *lock) {
	lock->stat = 0;
}
