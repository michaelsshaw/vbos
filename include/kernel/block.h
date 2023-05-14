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

	struct gpt_header *gpt_header;
	struct gpt_entry *gpt_entries;
	size_t gpt_entries_count;
};

struct block_device *block_register(char *name, struct block_device_ops *ops, void *data, size_t block_count, size_t block_size);
int block_read(struct block_device *bdev, void *buf, size_t offset, size_t size);
int block_write(struct block_device *bdev, void *buf, size_t offset, size_t size);

extern struct block_device *block_devices[];
extern size_t block_devices_count;

#endif /* _BLOCK_H_ */
