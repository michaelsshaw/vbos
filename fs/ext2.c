/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>

#include <fs/ext2.h>

static int ext2_read_super(struct block_device *bdev, struct ext2_superblock *sb_struct)
{
	char *buf = kmalloc(1024, ALLOC_DMA);
	if (!buf)
		return -1;

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Reading superblock from device %s, start_lba: %X\n", bdev->name, bdev->lba_start);
#endif

	int ret = block_read(bdev, buf, 2, 2);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	struct ext2_superblock *sb = (struct ext2_superblock *)buf;
	if (sb->magic != EXT2_SUPER_MAGIC) {
		kfree(buf);
		return -1;
	}

	memcpy(sb_struct, buf, sizeof(*sb_struct));
	kfree(buf);

	return ret;
}

/* PCD must be 1 for buf */
int ext2_read_block(struct ext2fs *fs, void *buf, size_t block)
{
	uint32_t bdev_block_size = fs->bdev->block_size;
	uint32_t num_blocks_read = fs->block_size / bdev_block_size;

	uint32_t bdev_block = block * num_blocks_read;

	return block_read(fs->bdev, buf, bdev_block, num_blocks_read);
}

int ext2_write_block(struct ext2fs *fs, void *buf, size_t block)
{
	uint32_t bdev_block_size = fs->bdev->block_size;
	uint32_t num_blocks_read = fs->block_size / bdev_block_size;

	uint32_t bdev_block = block * num_blocks_read;

	return block_write(fs->bdev, buf, bdev_block, num_blocks_read);
}

struct ext2fs *ext2_init_fs(struct block_device *bdev)
{
	struct ext2fs *ret = kmalloc(sizeof(*ret), ALLOC_KERN);

	if (!ret)
		return NULL;

	if (ext2_read_super(bdev, &ret->sb) < 0) {
		kfree(ret);
		return NULL;
	}

	ret->bdev = bdev;
	ret->block_size = 1024 << ret->sb.log_block_size;

	bdev->fs = ret;

	return ret;
}
