/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_MMAP 9
#define SYS_MUNMAP 11
#define SYS_FORK 57
#define SYS_EXIT 60
#define SYS_MKDIR 83
#define SYS_MKNOD 133
#define SYS_LSDIR 254

#include <kernel/common.h>
#include <kernel/proc.h>

void sys_set_return(struct proc *proc, uint64_t ret);
void sswtch(); /* save and switch */

#endif /* _SYSCALL_H_ */
