#include <kernel/common.h>
#include <kernel/lock.h>

#include <kernel/proc.h>

static uint64_t spinlock_attempt_acquire(spinlock_t *lock)
{
	return atomic_cmpxchg(lock, 0, 1);
}

void spinlock_acquire(spinlock_t *lock)
{
	while (spinlock_attempt_acquire(lock))
		;
}

void spinlock_release(spinlock_t *lock)
{
	atomic_cmpxchg(lock, 1, 0);
}

void mtx_acquire(mtx_t *mtx)
{
	while (spinlock_attempt_acquire(mtx))
		sswtch();
}

void mtx_release(mtx_t *mtx)
{
	spinlock_release(mtx);
}
