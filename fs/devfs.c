/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <fs/vfs.h>

static int devfs_read(struct vnode *vnode, void *buf, size_t offset, size_t size)
{
	return 0;
}

static int devfs_write(struct vnode *vnode, void *buf, size_t offset, size_t size)
{
	return 0;
}

/* devfs does not support unlink */
static int devfs_unlink(struct fs *fs, const char *path)
{
	return -EINVAL;
}

/* vnodes are permanent and therefore these functions should never be called */
static int devfs_open_vno(struct fs *fs, struct vnode *out, ino_t ino_num)
{
	return -EINVAL;
}

static int devfs_readdir(struct vnode *vnode, struct dirent **dir)
{
	return -EINVAL;
}

static int devfs_open_dir(struct fs *fs, struct vnode *out)
{
	return -EINVAL;
}

void devfs_init()
{
	struct fs *fs = vfs_create();
	if (!fs)
		goto out;

	struct fs_ops *ops = kmalloc(sizeof(struct fs_ops), ALLOC_KERN);
	if (!ops)
		goto out_ops;

	ops->read = devfs_read;
	ops->write = devfs_write;
	ops->unlink = devfs_unlink;
	ops->open_vno = devfs_open_vno;
	ops->readdir = devfs_readdir;
	ops->open_dir = devfs_open_dir;

	return;

out_ops:
	vfs_dealloc(fs);
out:
	kprintf(LOG_ERROR "devfs: failed to mount devfs\n");
	panic();
}
