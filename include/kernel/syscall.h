/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_FORK 57
#define SYS_EXIT 60

#include <kernel/common.h>
#include <kernel/proc.h>

void sys_set_return(struct proc *proc, uint64_t ret);

#endif /* _SYSCALL_H_ */
