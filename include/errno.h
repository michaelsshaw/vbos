/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ERRNO_H_
#define _ERRNO_H_

#define ENOMEM 1
#define ENOENT 2
#define EINVAL 3
#define EBADF 4

const char *strerror(int errnum);

#endif /* _ERRNO_H_ */
