/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <kernel/common.h>

struct block_device;

struct block_device_ops {
	int (*read)(struct block_device *bdev, void *buf, size_t offset, size_t size);
	int (*write)(struct block_device *bdev, void *buf, size_t offset, size_t size);
};

struct block_device {
	char *name;
	struct block_device_ops ops;
	void *data;
	size_t size;
};

struct block_device *block_register(char *name, struct block_device_ops *ops, void *data);
int block_read(struct block_device *bdev, void *buf, size_t offset, size_t size);
int block_write(struct block_device *bdev, void *buf, size_t offset, size_t size);

#endif /* _BLOCK_H_ */
