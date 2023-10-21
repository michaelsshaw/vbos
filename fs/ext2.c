/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <dev/console.h>

#include <fs/ext2.h>
#include <fs/vfs.h>

#define ITYPE_DECL(_ino, _de) [(_ino >> 12)] = (_de)
static uint8_t inode_to_ftype[] = {
	ITYPE_DECL(EXT2_INO_FIFO, VFS_FILE_FIFO), ITYPE_DECL(EXT2_INO_CHARDEV, VFS_FILE_CHARDEV),
	ITYPE_DECL(EXT2_INO_DIR, VFS_FILE_DIR),	  ITYPE_DECL(EXT2_INO_BLKDEV, VFS_FILE_BLKDEV),
	ITYPE_DECL(EXT2_INO_REG, VFS_FILE_FILE),  ITYPE_DECL(EXT2_INO_SYMLINK, VFS_FILE_SYMLINK),
	ITYPE_DECL(EXT2_INO_SOCK, VFS_FILE_SOCK),
};
#undef ITYPE_DECL

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

/* nasty function, but it works */
static int ext2_inode_read_block(struct ext2fs *fs, struct ext2_inode *inode, void *buf, size_t ino_block)
{
	if (ino_block < 12)
		return ext2_read_block(fs, buf, inode->block[ino_block]);

	if (ino_block < 12 + fs->indir_block_size) {
		uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf)
			return -ENOMEM;

		ext2_read_block(fs, indir_buf, inode->block[12]);

		int ret = ext2_read_block(fs, buf, indir_buf[ino_block - 12]);
		kfree(indir_buf);

		return ret;
	}

	if (ino_block < 12 + fs->indir_block_size + fs->indir_block_size * fs->indir_block_size) {
		uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf)
			return -ENOMEM;

		ext2_read_block(fs, indir_buf, inode->block[13]);

		uint32_t *indir_buf2 = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf2) {
			kfree(indir_buf);
			return -ENOMEM;
		}

		ext2_read_block(fs, indir_buf2, indir_buf[(ino_block - 12 - fs->indir_block_size) / fs->indir_block_size]);

		int ret = ext2_read_block(fs, buf, indir_buf2[(ino_block - 12 - fs->indir_block_size) % fs->indir_block_size]);
		kfree(indir_buf);
		kfree(indir_buf2);

		return ret;
	}

	if (ino_block < 12 + fs->indir_block_size + fs->indir_block_size * fs->indir_block_size +
				fs->indir_block_size * fs->indir_block_size * fs->indir_block_size) {
		uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf)
			return -ENOMEM;

		ext2_read_block(fs, indir_buf, inode->block[14]);

		uint32_t *indir_buf2 = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf2) {
			kfree(indir_buf);
			return -ENOMEM;
		}

		ext2_read_block(fs, indir_buf2,
				indir_buf[(ino_block - 12 - fs->indir_block_size - fs->indir_block_size * fs->indir_block_size) /
					  (fs->indir_block_size * fs->indir_block_size)]);

		uint32_t *indir_buf3 = kmalloc(fs->block_size, ALLOC_DMA);
		if (!indir_buf3) {
			kfree(indir_buf);
			kfree(indir_buf2);
			return -ENOMEM;
		}

		ext2_read_block(fs, indir_buf3,
				indir_buf2[((ino_block - 12 - fs->indir_block_size - fs->indir_block_size * fs->indir_block_size) %
					    (fs->indir_block_size * fs->indir_block_size)) /
					   fs->indir_block_size]);

		int ret = ext2_read_block(
			fs, buf,
			indir_buf3[((ino_block - 12 - fs->indir_block_size - fs->indir_block_size * fs->indir_block_size) %
				    (fs->indir_block_size * fs->indir_block_size)) %
				   fs->indir_block_size]);
		kfree(indir_buf);
		kfree(indir_buf2);
		kfree(indir_buf3);

		return ret;
	}

	return -EINVAL;
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

int ext2_open_file(struct fs *vfs, struct file *file, const char *path)
{
	/* find the inode of the file */
	struct ext2fs *fs = (struct ext2fs *)vfs->fs;
	struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
	if (!inode)
		return -ENOMEM;

	uint32_t inode_no = 2;
	int ret = ext2_read_inode(fs, inode, inode_no);
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
	struct ext2_dir_entry *entry = kmalloc(fs->block_size, ALLOC_DMA);
	void *oentry = entry;
	while (token) {
		if (!entry) {
			kfree(inode);
			kfree(path_copy);
			return -ENOMEM;
		}

		for (int i = 0; i < inode->blocks; i++) {
			/* since this inode is a directory, we just read the blocks
			 * sequentially until we find the file we're looking for
			 */

			ext2_inode_read_block(fs, inode, entry, i);

			uint32_t rec_offset = 0;
			while (entry->inode && rec_offset < fs->block_size) {
				inode_no = entry->inode;
				if (!strncmp(entry->name, token, MIN(entry->name_len, strlen(token))))
					goto found;

				entry = (void *)entry + entry->rec_len;
				rec_offset += entry->rec_len;
			}
		}

		if (!entry || !entry->inode) {
			kfree(inode);
			kfree(path_copy);
			kfree(oentry);
			return -ENOENT;
		}

found:
		ext2_read_inode(fs, inode, entry->inode);

		token = strtok(NULL, "/", &strtok_last);
	}

	kfree(path_copy);

	memcpy(&file->inode, inode, sizeof(struct inode));
	file->size = inode->size;
	file->type = inode_to_ftype[inode->mode >> 12];
	file->inode_num = inode_no;
	kfree(oentry);

	/* check for long filesize */
	if (fs->sb.feature_ro_compat & EXT2_FLAG_LONG_FILESIZE)
		file->size |= ((uint64_t)inode->dir_acl << 32);

	return 0;
}

int ext2_readdir(struct fs *fs, uint32_t ino_num, struct dirent **out)
{
	struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
	if (!inode)
		return -ENOMEM;

	int ret = ext2_read_inode(fs->fs, inode, ino_num);
	if (ret < 0) {
		kfree(inode);
		return ret;
	}

	if ((inode->mode & 0xF000) != EXT2_INO_DIR) {
		kfree(inode);
		return -ENOTDIR;
	}

	struct ext2fs *extfs = (struct ext2fs *)fs->fs;
	struct ext2_dir_entry *entry = kmalloc(extfs->block_size, ALLOC_DMA);
	void *oentry = entry;
	if (!entry) {
		kfree(inode);
		return -ENOMEM;
	}

	/* populate the dirent array with all the entries in the inode */
	int num_entries = 0;
	size_t dir_arr_size = inode->blocks * extfs->block_size;
	dir_arr_size /= sizeof(struct dirent);

	struct dirent *dir_arr = kmalloc(dir_arr_size * sizeof(struct dirent), ALLOC_KERN);
	if (!dir_arr) {
		kfree(inode);
		kfree(entry);
		return -ENOMEM;
	}

	for (int i = 0; i < inode->blocks; i++) {
		ext2_inode_read_block(extfs, inode, entry, i);

		uint32_t rec_offset = 0;
		while (entry->inode && rec_offset < extfs->block_size) {
			struct dirent *dirent = &dir_arr[num_entries];

			dirent->inode = entry->inode;
			dirent->type = inode_to_ftype[entry->file_type];
			dirent->reclen = entry->rec_len;

			memcpy(dirent->name, entry->name, MIN(255, entry->name_len));
			dirent->name[MIN(255, entry->name_len)] = '\0';

			num_entries++;

			entry = (void *)entry + entry->rec_len;
			rec_offset += entry->rec_len;
		}
	}

	kfree(inode);
	kfree(oentry);

	*out = dir_arr;
	return num_entries;
}

int ext2_close_file(struct fs *vfs, struct file *file)
{
	return 0;
}

int ext2_write_file(struct fs *vfs, struct file *file, void *buf, size_t offset, size_t count)
{
	return 0;
}

int ext2_read_file(struct fs *vfs, struct file *file, void *buf, size_t offset, size_t count)
{
	struct ext2fs *fs = vfs->fs;

	if ((file->inode.mode & 0xF000) != EXT2_INO_REG)
		return -EISDIR;

	size_t block = offset / fs->block_size;
	offset %= fs->block_size;

	if (offset > file->size)
		return -EINVAL;

	if (offset + count > file->size)
		count = (file->size - offset);

	size_t ret = count;

	void *block_buf = kmalloc(fs->block_size, ALLOC_KERN);

	if (!block_buf)
		return -ENOMEM;

	size_t bufoffset = 0;

	while (count > 0) {
		int retval = ext2_inode_read_block(fs, (struct ext2_inode *)&file->inode, block_buf, block);
		if (retval < 0) {
			kfree(block_buf);
			return retval;
		}

		memcpy(buf + bufoffset, block_buf + offset, MIN(count, fs->block_size - offset));
		bufoffset += fs->block_size;
		offset = 0;

		count -= MIN(fs->block_size, count);
	}

	return ret;
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
	extfs->indir_block_size = extfs->block_size / sizeof(uint32_t);

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
		.readdir = ext2_readdir,
	};

	ret->ops = ops;

	return ret;
}
