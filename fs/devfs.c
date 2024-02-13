/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <kernel/block.h>
#include <fs/vfs.h>

struct fs *devfs;

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

int devfs_insert(struct vnode *dir, const char *name, uint32_t flags)
{
	if (!dir || !name)
		return -EINVAL;

	struct vnode *dev_vnode = vfs_create_vno();
	if (!dev_vnode)
		return -ENOMEM;

	dev_vnode->fs = dir->fs;
	dev_vnode->flags = flags;
	dev_vnode->no_free = true;

	spinlock_acquire(&dir->lock);

	dir->num_dirents++;
	dir->dirents = krealloc(dir->dirents, dir->num_dirents * sizeof(struct dirent), ALLOC_KERN);

	struct dirent *dirent = &dir->dirents[dir->num_dirents - 1];

	memcpy(dirent->name, name, MIN(sizeof(dirent->name) - 1, strlen(name) + 1));
	dirent->name[MIN(sizeof(dirent->name) - 1, strlen(name))] = '\0';
	dirent->vnode = dev_vnode;
	dirent->reclen = sizeof(struct dirent);
	dirent->inode = 0;

	spinlock_release(&dir->lock);

	return 0;
}

struct fs *devfs_init()
{
	devfs = vfs_create();
	if (!devfs)
		goto out;

	devfs->ops->read = devfs_read;
	devfs->ops->write = devfs_write;
	devfs->ops->unlink = devfs_unlink;
	devfs->ops->open_vno = devfs_open_vno;
	devfs->ops->readdir = devfs_readdir;
	devfs->ops->open_dir = devfs_open_dir;

	devfs->root->flags = VFS_VNO_DIR;

	int res = vfs_mount(devfs, "dev");

	if(res < 0) {
		kprintf(LOG_ERROR "devfs: failed to mount devfs: %s\n", strerror(-res));
		goto out;
	}

	/* get all block devices */
	struct block_device **block_get_all_devices(int *n);

	int n;
	struct block_device **bdevs = block_get_all_devices(&n);

	if (n > 0) {
		for (int i = 0; i < n; i++) {
			devfs_insert(devfs->root, bdevs[i]->name, VFS_VNO_BLKDEV);
		}
	}

	return devfs;
out:
	kprintf(LOG_ERROR "devfs: failed to mount devfs\n");
	panic();

	return NULL; /* cannot possibly happen */
}
