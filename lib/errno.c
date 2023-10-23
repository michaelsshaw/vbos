/* SPDX-License-Identifier: GPL-2.0-only */
#include <errno.h>
#include <kernel/common.h>

static const char *errno_str[] = {
	[0] = "Success",
	[ENOMEM] = "Out of memory",
	[ENOENT] = "No such file or directory",
	[EINVAL] = "Invalid argument",
	[EBADF] = "Bad file descriptor",
	[ENOTDIR] = "Not a directory",
	[EISDIR] = "Is a directory",
	[ENOSPC] = "No space left on device",
	[EEXIST] = "File exists",
};

inline const char *strerror(int errnum)
{
	if (errnum < 0 || (size_t)errnum >= ARRAY_SIZE(errno_str))
		return "Unknown error";

	return errno_str[errnum];
}
