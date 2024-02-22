/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SEM_H_
#define _SEM_H_

#include <kernel/lock.h>

typedef struct {
	int count;
	spinlock_t lock;
} sem_t;

sem_t *sem_create(int count);
void sem_destroy(sem_t *sem);

int sem_trywait(sem_t *sem);
void sem_post(sem_t *sem);

#endif /* _SEM_H_ */
