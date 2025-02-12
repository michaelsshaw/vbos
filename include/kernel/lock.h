/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LOCK_H_
#define _LOCK_H_

typedef int spinlock_t;
typedef spinlock_t mtx_t;

int atomic_cmpxchg(volatile int *ptr, int cmpval, int newval);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);

void mtx_acquire(mtx_t *mtx);
void mtx_release(mtx_t *mtx);

#endif /* _LOCK_H_ */
