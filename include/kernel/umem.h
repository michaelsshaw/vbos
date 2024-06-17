/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _UMEM_H_
#define _UMEM_H_

#include <kernel/proc.h>

void copy_to_user(struct proc *proc, void *dst, const void *src, size_t size);

#endif /* _UMEM_H_ */
