/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <dev/console.h>

#include <fs/ext2.h>
#include <fs/vfs.h>

static int ext2_read_super(struct block_device *bdev, struct ext2_superblock *sb_struct)
{
	char *buf = kmalloc(1024, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

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
		return -EINVAL;
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

int ext2_read_inode(struct ext2fs *fs, struct ext2_inode *out, uint32_t inode)
{
	uint32_t group = (inode - 1) / fs->sb.inodes_per_group;
	uint32_t index = (inode - 1) % fs->sb.inodes_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[group];

	uint32_t block = bg->inode_table + (index * sizeof(struct ext2_inode)) / fs->block_size;
	uint32_t offset = (index * sizeof(struct ext2_inode)) % fs->block_size;

	char *buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	int ret = ext2_read_block(fs, buf, block);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	memcpy(out, buf + offset, sizeof(struct ext2_inode));

	kfree(buf);

	return 0;
}

int ext2_write_inode(struct ext2fs *fs, struct ext2_inode *in, uint32_t inode)
{
	uint32_t group = (inode - 1) / fs->sb.inodes_per_group;
	uint32_t index = (inode - 1) % fs->sb.inodes_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[group];

	uint32_t block = bg->inode_table + (index * sizeof(struct ext2_inode)) / fs->block_size;
	uint32_t offset = (index * sizeof(struct ext2_inode)) % fs->block_size;

	char *buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	int ret = ext2_read_block(fs, buf, block);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	memcpy(buf + offset, in, sizeof(struct ext2_inode));

	ret = ext2_write_block(fs, buf, block);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	kfree(buf);

	return 0;
}

int ext2_open_file(void *extfs, struct file *file, const char *path)
{
	/* find the inode of the file */
	struct ext2fs *fs = (struct ext2fs *)extfs;
	struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
	if (!inode)
		return -ENOMEM;

	int ret = ext2_read_inode(fs, inode, 2);
	if (ret < 0) {
		kfree(inode);
		return ret;
	}

	char *path_copy = kmalloc(strlen(path) + 1, ALLOC_KERN);
	if (!path_copy) {
		kfree(inode);
		return -ENOMEM;
	}

	strcpy(path_copy, path);

	char *strtok_last = NULL;
	char *token = strtok(path_copy, "/", &strtok_last);
	uint32_t inode_no = 2;
	while (token) {
		struct ext2_dir_entry *entry = kmalloc(fs->block_size, ALLOC_DMA);
		if (!entry) {
			kfree(inode);
			kfree(path_copy);
			return -ENOMEM;
		}

		ext2_read_block(fs, entry, inode->block[0]);

		while (entry->inode) {
			char *name = kmalloc(entry->name_len + 1, ALLOC_KERN);
			if (!name) {
				kfree(inode);
				kfree(path_copy);
				kfree(entry);
				return -ENOMEM;
			}

			memcpy(name, entry->name, entry->name_len);
			name[entry->name_len] = '\0';

			if (!strcmp(name, token)) {
				kfree(name);
				break;
			}

			kfree(name);

			inode_no = entry->inode;
			entry = (void *)entry + entry->rec_len;
		}

		if (!entry->inode) {
			kfree(inode);
			kfree(path_copy);
			kfree(entry);
			return -ENOENT;
		}

		ext2_read_inode(fs, inode, entry->inode);

		token = strtok(NULL, "/", &strtok_last);
	}


	kfree(path_copy);

	memcpy(&file->inode, inode, sizeof(struct inode));
	file->size = inode->size;
	file->inode_num = inode_no;

	/* check for long filesize */
	if (fs->sb.feature_ro_compat & EXT2_FLAG_LONG_FILESIZE)
		file->size |= ((uint64_t)inode->dir_acl << 32);

	return 0;
}

int ext2_close_file(void *extfs, struct file *file)
{
	return 0;
}

int ext2_write_file(void *extfs, struct file *file, void *buf, size_t count)
{
	return 0;
}

int ext2_read_file(void *extfs, struct file *file, void *buf, size_t count)
{
	return 0;
}

int ext2_seek_file(void *extfs, struct file *file, size_t offset)
{
	return 0;
}

struct fs *ext2_init_fs(struct block_device *bdev)
{
	struct ext2fs *extfs = kmalloc(sizeof(*extfs), ALLOC_KERN);

	if (!extfs)
		return NULL;

	if (ext2_read_super(bdev, &extfs->sb) < 0) {
		kfree(extfs);
		return NULL;
	}

	extfs->bdev = bdev;
	extfs->block_size = 1024 << extfs->sb.log_block_size;

	bdev->fs = extfs;

	/* read block group descriptor table */
	uint32_t num_groups = extfs->sb.blocks_count / extfs->sb.blocks_per_group;
	if (extfs->sb.blocks_count % extfs->sb.blocks_per_group)
		num_groups++;

	uint32_t bgdt_block = 2 + (extfs->sb.first_data_block / extfs->sb.blocks_per_group);
	uint32_t bgdt_size = num_groups * sizeof(struct ext2_group_desc);

	char *buf = kmalloc(bgdt_size, ALLOC_DMA);

	ext2_read_block(bdev->fs, buf, bgdt_block);

	extfs->bgdt = (struct ext2_group_desc *)kmalloc(bgdt_size, ALLOC_KERN);
	memcpy(extfs->bgdt, buf, bgdt_size);

	kfree(buf);

	struct fs *ret = kmalloc(sizeof(struct fs), ALLOC_KERN);
	ret->fs = extfs;
	ret->type = VFS_TYPE_EXT2;

	struct fs_ops ops = {
		.open = ext2_open_file,
		.close = ext2_close_file,
		.read = ext2_read_file,
		.write = ext2_write_file,
		.seek = ext2_seek_file,
	};

	ret->ops = ops;

	return ret;
}
