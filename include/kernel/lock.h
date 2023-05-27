/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LOCK_H_
#define _LOCK_H_

typedef int spinlock_t;

int atomic_cmpxchg(volatile int *ptr, int cmpval, int newval);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

#endif /* _LOCK_H_ */
