/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <dev/console.h>

#include <fs/vfs.h>

int stdout_write(struct file_descriptor *fdesc, void *buf, size_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		console_write(((char *)buf)[i]);
	}
	return i;
}
