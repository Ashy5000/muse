#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

#define LSTAT_WRITE 1
#define LSTAT_READ  2

struct lock_simple {
	uint32_t stat;
};

void lock_simple_acquire(volatile struct lock_simple *lock);
void lock_simple_release(volatile struct lock_simple *lock);

struct lock_reentrant {
	uint32_t cnt;
	uint32_t owner;
};

void lock_reentrant_acquire(volatile struct lock_reentrant *lock);
void lock_reentrant_release(volatile struct lock_reentrant *lock);

struct lock_multi {
	uint32_t waiting_readers;
	uint32_t stat;
};

void lock_multi_acquire_read(volatile struct lock_multi *lock);
void lock_multi_wait(volatile struct lock_multi *lock);
void lock_multi_release_read(volatile struct lock_multi *lock);
void lock_multi_acquire_write(volatile struct lock_multi *lock);
void lock_multi_signal(volatile struct lock_multi *lock);
void lock_multi_release_write(volatile struct lock_multi *lock);

#endif
