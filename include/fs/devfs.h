/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _DEVFS_H_
#define _DEVFS_H_

#include <kernel/common.h>

struct vnode;
struct devfs_dev_info {
	void *dev;
	ssize_t (*read)(void *dev, void *buf, size_t offset, size_t size);
	ssize_t (*write)(void *dev, void *buf, size_t offset, size_t size);
};

struct fs *devfs_init();
int devfs_insert(struct vnode *dir, const char *name, uint32_t flags, struct devfs_dev_info *ops);

#endif /* _DEVFS_H_ */
