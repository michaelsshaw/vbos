/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ERRNO_H_
#define _ERRNO_H_


#define ENOMEM 1
#define ENOENT 2
#define EINVAL 3
#define EBADF 4
#define ENOTDIR 5
#define EISDIR 6
#define ENOSPC 7 
#define EEXIST 8
#define ENOEXEC 9
#define EFAULT 10
#define EAGAIN 11
#define EBUSY 12
#define ENOSYS 13
#define ESYSCALLBLK 14

#ifndef __ASM__

const char *strerror(int errnum);

#endif /* __ASM__ */
#endif /* _ERRNO_H_ */
