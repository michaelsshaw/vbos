#include <kernel/common.h>
#include <kernel/lock.h>

void spinlock_acquire(spinlock_t *lock)
{
	while (atomic_cmpxchg(lock, 0, 1))
		;
}

void spinlock_release(spinlock_t *lock)
{
	atomic_cmpxchg(lock, 1, 0);
}
