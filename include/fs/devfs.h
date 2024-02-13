/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _DEVFS_H_
#define _DEVFS_H_

#include <kernel/common.h>

typedef ssize_t (*devfs_dev_ops_write)(void *dev, void *buf, size_t offset, size_t size);
typedef ssize_t (*devfs_dev_ops_read)(void *dev, void *buf, size_t offset, size_t size);

struct vnode;
struct devfs_dev_ops {
	devfs_dev_ops_write write;
	devfs_dev_ops_read read;
};

struct fs *devfs_init();
int devfs_insert(struct vnode *dir, const char *name, uint32_t flags, struct devfs_dev_ops *ops);

#endif /* _DEVFS_H_ */
