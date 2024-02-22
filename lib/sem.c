/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <lib/sem.h>

sem_t *sem_create(int count)
{
	sem_t *sem = kmalloc(sizeof(sem_t), ALLOC_KERN);
	sem->count = count;
	sem->lock = 0;
	return sem;
}

void sem_destroy(sem_t *sem)
{
	ATTEMPT_FREE(sem);
}

int sem_trywait(sem_t *sem)
{
	int ret = 1;
	spinlock_acquire(&sem->lock);

	if (sem->count > 0) {
		sem->count--;
		ret = 0;
	}

	spinlock_release(&sem->lock);
	return ret;
}

void sem_post(sem_t *sem)
{
	spinlock_acquire(&sem->lock);
	sem->count++;
	spinlock_release(&sem->lock);
}
