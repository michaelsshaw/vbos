/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>

#include <fs/vfs.h>

static struct rbtree *kfd;
static struct fs *rootfs;

typedef struct fs *(*vfs_init_t)(struct block_device *);

void vfs_init(const char *rootdev_name)
{
	kfd = kzalloc(sizeof(struct rbtree), ALLOC_DMA);

	/* mount root */
	struct block_device *rootdev = block_get_device(rootdev_name);
	if (!rootdev) {
		kprintf(LOG_ERROR "Failed to find root device %s\n", rootdev_name);
		return;
	} else {
		kprintf(LOG_DEBUG "Found root device %s\n", rootdev_name);

		/* TODO: detect filesystem type */
		/* for now, assume ext2 */
		struct fs *fs = ext2_init_fs(rootdev);
		if (!fs) {
			kprintf(LOG_ERROR "Failed to mount root device %s\n", rootdev_name);
			return;
		} else {
			kprintf(LOG_DEBUG "Mounted root device %s\n", rootdev_name);
		}

		fs->mount_point = kzalloc(2, ALLOC_KERN);
		fs->mount_point[0] = '/';

		rootfs = fs;
	}
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
