/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/block.h>
#include <kernel/slab.h>
#include <kernel/gpt.h>

#define MAX_BLOCK_DEVICES 24

struct block_device *block_devices[MAX_BLOCK_DEVICES];
size_t block_devices_count = 0;

struct block_device *block_register(char *name, struct block_device_ops *ops, void *data, size_t block_count, size_t block_size)
{
	struct block_device *bdev;

	bdev = kmalloc(sizeof(struct block_device), ALLOC_KERN);
	if (!bdev)
		return NULL;

	bdev->name = name;
	bdev->data = data;
	bdev->block_count = block_count;
	bdev->block_size = block_size;
	bdev->lba_start = 0;
	bdev->fs = NULL;
	memcpy(&bdev->ops, ops, sizeof(struct block_device_ops));

	block_devices[block_devices_count] = bdev;
	block_devices_count++;

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Registered block device %s\n", name);
#endif

	return bdev;
}

struct block_device *block_get_device(const char *name)
{
	for (size_t i = 0; i < block_devices_count; i++) {
		if (!strcmp(block_devices[i]->name, name))
			return block_devices[i];
	}

	return NULL;
}

inline int block_read(struct block_device *bdev, void *buf, size_t offset, size_t size)
{
	return bdev->ops.read(bdev, buf, offset + bdev->lba_start, size);
}

inline int block_write(struct block_device *bdev, void *buf, size_t offset, size_t size)
{
	return bdev->ops.write(bdev, buf, offset + bdev->lba_start, size);
}
