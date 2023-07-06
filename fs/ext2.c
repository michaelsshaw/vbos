/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>

#include <fs/ext2.h>

static int ext2_read_super(struct block_device *bdev, struct ext2_superblock *sb_struct)
{
	char *buf = kmalloc(1024, ALLOC_DMA);
	if (!buf)
		return -1;

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
	return block_read(fs->bdev, buf, block * (fs->block_size >> 9), fs->block_size >> 9);
}

int ext2_write_block(struct ext2fs *fs, void *buf, size_t block)
{
	return block_write(fs->bdev, buf, block * (fs->block_size >> 9), fs->block_size >> 9);
}

int ext2_read_inode(struct ext2fs *fs, struct ext2_inode *out, size_t inode_no)
{
	size_t group = (inode_no - 1) / fs->sb.inodes_per_group;
	size_t index = (inode_no - 1) % fs->sb.inodes_per_group;

	struct ext2_group_desc *gd = kmalloc(fs->block_size, ALLOC_DMA);
	if (!gd)
		return -1;

	int ret = ext2_read_block(fs, gd, fs->sb.first_data_block + 1 + group);
	if (ret < 0) {
		kfree(gd);
		return ret;
	}

	size_t block = gd->inode_table + index / (fs->block_size / sizeof(struct ext2_inode));
	size_t offset = index % (fs->block_size / sizeof(struct ext2_inode));

	struct ext2_inode *inodes = kmalloc(fs->block_size, ALLOC_DMA);
	if (!inodes) {
		kfree(gd);
		return -1;
	}

	ret = ext2_read_block(fs, inodes, block);
	if (ret < 0) {
		kfree(gd);
		kfree(inodes);
		return ret;
	}

	memcpy(out, &inodes[offset], sizeof(*out));

	kfree(gd);
	kfree(inodes);

	return 0;
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
