/* SDPX-License-Identifier: GPL-2.0-only */

#include <fs/vfs.h>
#include <kernel/slab.h>
#include <kernel/block.h>

void vfs_init(struct block_device *root)
{

}

int write(int fd, const void *buf, size_t count)
{
	return 0;
}

int read(int fd, void *buf, size_t count)
{
	return 0;
}

int open(const char *pathname, int flags)
{
	return 0;
}

int close(int fd)
{
	return 0;
}
