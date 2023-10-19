/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _ERRNO_H_
#define _ERRNO_H_

#define ENOMEM 1
#define ENOENT 2
#define EINVAL 3
#define EBADF 4

static const char *errno_str[] = {
	[0] = "Success",
	[ENOMEM] = "Out of memory",
	[ENOENT] = "No such file or directory",
	[EINVAL] = "Invalid argument",
	[EBADF] = "Bad file descriptor",
};

#endif /* _ERRNO_H_ */
