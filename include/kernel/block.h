/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <kernel/common.h>
#include <kernel/gpt.h>

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

	size_t block_count;
	size_t block_size;
	size_t lba_start;

	size_t partition_count;

	void *fs;
	struct block_device *parent;
	struct block_device *next_part; /* singly linked list */
};

struct block_device *block_register(char *name, struct block_device_ops *ops, void *data, size_t block_count, size_t block_size);
struct block_device *block_get_device(const char *name);
int block_read(struct block_device *bdev, void *buf, size_t offset, size_t size);
int block_write(struct block_device *bdev, void *buf, size_t offset, size_t size);
void kerror_print_blkdevs();

extern struct block_device *block_devices[];
extern size_t block_devices_count;

#endif /* _BLOCK_H_ */
