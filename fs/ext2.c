/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>

#include <fs/ext2.h>
#include <fs/vfs.h>

#define N_DIRECT 12
#define N_INDIR (fs->block_size / sizeof(uint32_t))
#define N_DINDIR (N_INDIR * N_INDIR)
#define N_TINDIR (N_DINDIR * N_INDIR + N_DINDIR + N_INDIR)

#define N_PER_INDIRECT (fs->block_size / sizeof(uint32_t))

#define ITYPE_DECL(_ino, _de) [(_ino >> 12)] = (_de)
#define DTYPE_DECL(_de, _ino) [(_de)] = (_ino)

static struct fs_ops *ext2_ops;

static uint8_t inode_to_ftype[] = {
	ITYPE_DECL(EXT2_INO_FIFO, EXT2_DE_FIFO), ITYPE_DECL(EXT2_INO_CHARDEV, EXT2_DE_CHRDEV),
	ITYPE_DECL(EXT2_INO_DIR, EXT2_DE_DIR),	 ITYPE_DECL(EXT2_INO_BLKDEV, EXT2_DE_BLKDEV),
	ITYPE_DECL(EXT2_INO_REG, EXT2_DE_FILE),	 ITYPE_DECL(EXT2_INO_SYMLINK, EXT2_DE_SYMLINK),
	ITYPE_DECL(EXT2_INO_SOCK, EXT2_DE_SOCK),
};

#undef ITYPE_DECL
#undef DTYPE_DECL

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
static int ext2_read_block(struct ext2fs *fs, void *buf, size_t block)
{
	uint32_t bdev_block_size = fs->bdev->block_size;
	uint32_t num_blocks_read = fs->block_size / bdev_block_size;

	uint32_t bdev_block = block * num_blocks_read;

	return block_read(fs->bdev, buf, bdev_block, num_blocks_read);
}

static long ext2_block_single_indir(struct ext2fs *fs, uint32_t dir_block, size_t index)
{
	uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!indir_buf)
		return -ENOMEM;

	ext2_read_block(fs, indir_buf, dir_block);

	long ret = indir_buf[index];

	kfree(indir_buf);

	return ret;
}

static long ext2_block_double_indir(struct ext2fs *fs, uint32_t dir_block, size_t index, size_t index_2)
{
	uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!indir_buf)
		return -ENOMEM;

	ext2_read_block(fs, indir_buf, dir_block);

	return ext2_block_single_indir(fs, indir_buf[index], index_2);
}

static long ext2_inode_get_block(struct ext2fs *fs, struct ext2_inode *inode, size_t ino_block)
{
	if (ino_block < N_DIRECT)
		return inode->block[ino_block];

	if (ino_block < N_DIRECT + N_INDIR)
		return ext2_block_single_indir(fs, inode->block[N_DIRECT], ino_block - N_DIRECT);

	if (ino_block < N_DIRECT + N_INDIR + N_DINDIR) {
		size_t index = (ino_block - N_DIRECT - N_INDIR) / N_PER_INDIRECT;
		size_t index_2 = (ino_block - N_DIRECT - N_INDIR) % N_PER_INDIRECT;

		return ext2_block_double_indir(fs, inode->block[N_DIRECT + 1], index, index_2);
	}

	if (ino_block < N_DIRECT + N_INDIR + N_DINDIR + N_TINDIR) {
		size_t index = (ino_block - N_DIRECT - N_INDIR - N_DINDIR) / (N_PER_INDIRECT * N_PER_INDIRECT);
		size_t index_2 = ((ino_block - N_DIRECT - N_INDIR - N_DINDIR) % (N_PER_INDIRECT * N_PER_INDIRECT)) / N_PER_INDIRECT;
		size_t index_3 = ((ino_block - N_DIRECT - N_INDIR - N_DINDIR) % (N_PER_INDIRECT * N_PER_INDIRECT)) % N_PER_INDIRECT;

		long ret = ext2_block_double_indir(fs, inode->block[N_DIRECT + 2], index, index_2);
		ret = ext2_block_single_indir(fs, ret, index_3);
	}

	return -EINVAL;
}

static int ext2_inode_read_block(struct ext2fs *fs, struct ext2_inode *inode, void *buf, size_t ino_block)
{
	long ret = ext2_inode_get_block(fs, inode, ino_block);
	if (ret < 0)
		return ret;

	return ext2_read_block(fs, buf, ret);
}

static int ext2_write_block(struct ext2fs *fs, void *buf, size_t block)
{
	uint32_t bdev_block_size = fs->bdev->block_size;
	uint32_t num_blocks_read = fs->block_size / bdev_block_size;

	uint32_t bdev_block = block * num_blocks_read;

	return block_write(fs->bdev, buf, bdev_block, num_blocks_read);
}

static int ext2_write_bgdt(struct ext2fs *fs)
{
	for (size_t i = 0; i < fs->bgdt_num_blocks; i++) {
		void *buf = (void *)fs->bgdt + fs->block_size * i;
		int ret = ext2_write_block(fs, buf, fs->bgdt_blockno + i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ext2_write_super(struct ext2fs *fs)
{
	return ext2_write_block(fs, &fs->sb, 1);
}

static int ext2_inode_write_block(struct ext2fs *fs, struct ext2_inode *inode, void *buf, size_t ino_block)
{
	long ret = ext2_inode_get_block(fs, inode, ino_block);
	if (ret < 0)
		return ret;

	return ext2_write_block(fs, buf, ret);
}

static int ext2_read_inode(struct ext2fs *fs, struct ext2_inode *out, ino_t inode)
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

static int ext2_write_inode(struct ext2fs *fs, struct ext2_inode *in, ino_t inode)
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

static long ext2_alloc_block(struct ext2fs *fs)
{
	uint32_t num_groups = fs->sb.blocks_count / fs->sb.blocks_per_group;
	if (fs->sb.blocks_count % fs->sb.blocks_per_group)
		num_groups++;

	for (int i = 0; i < num_groups; i++) {
		struct ext2_group_desc *bg = &fs->bgdt[i];

		if (bg->free_blocks_count) {
			uint32_t block = bg->block_bitmap;
			uint32_t block_offset = 0;

			char *buf = kmalloc(fs->block_size, ALLOC_DMA);
			if (!buf)
				return -ENOMEM;

			ext2_read_block(fs, buf, block);

			while (block_offset < fs->block_size) {
				uint8_t *byte = (uint8_t *)buf + block_offset;
				if (*byte == 0xFF) {
					block_offset += 8;
					continue;
				}

				for (int j = 0; j < 8; j++) {
					if (!(*byte & (1 << j))) {
						*byte |= (1 << j);
						bg->free_blocks_count--;
						fs->sb.free_blocks_count--;

						ext2_write_block(fs, buf, block);

						/* update superblock and bgdt */
						ext2_write_super(fs);
						ext2_write_bgdt(fs);

						kfree(buf);

						uint32_t block_ret_no = (i * fs->sb.blocks_per_group) + (block_offset * 8) + j + 1;

						/* zero out the block */
						char *block_buf = kmalloc(fs->block_size, ALLOC_DMA);
						if (!block_buf)
							return -ENOMEM;

						memset(block_buf, 0, fs->block_size);
						ext2_write_block(fs, block_buf, block_ret_no);
						kfree(block_buf);

						return block_ret_no;
					}
				}

				block_offset++;
			}

			kfree(buf);
		}
	}

	return -ENOSPC;
}

static long ext2_alloc_block_indir(struct ext2fs *fs, uint32_t dir_block, size_t index)
{
	uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!indir_buf)
		return -ENOMEM;

	ext2_read_block(fs, indir_buf, dir_block);

	long ret = ext2_alloc_block(fs);
	if (ret < 0) {
		kfree(indir_buf);
		return ret;
	}

	indir_buf[index] = ret;
	ext2_write_block(fs, indir_buf, dir_block);
	kfree(indir_buf);

	return ret;
}

static long ext2_inode_alloc_block(struct ext2fs *fs, struct ext2_inode *inode, ino_t inode_no)
{
	/* allocate inode block and handle indirect if necessary */
	long block;
	size_t num_blocks = (inode->blocks * fs->bdev->block_size) / fs->block_size;

	if (num_blocks < N_DIRECT) {
		block = ext2_alloc_block(fs);
		inode->block[num_blocks] = block;
	} else if (num_blocks < N_DIRECT + N_INDIR) {
		size_t index = num_blocks - N_DIRECT;

		long indir_block = inode->block[N_DIRECT];
		if (index == 0) {
			indir_block = ext2_alloc_block(fs);
			inode->block[N_DIRECT] = indir_block;
		}

		block = ext2_alloc_block_indir(fs, indir_block, index);
	} else if (num_blocks < N_DIRECT + N_INDIR + N_DINDIR) {
		size_t index = (num_blocks - N_DIRECT - N_INDIR) / N_PER_INDIRECT;
		size_t index2 = (num_blocks - N_DIRECT - N_INDIR) % N_PER_INDIRECT;

		long dindir_block = inode->block[N_DIRECT + 1];
		if (index == 0) {
			dindir_block = ext2_alloc_block(fs);
			inode->block[N_DIRECT + 1] = dindir_block;
		}

		long indir_block = ext2_block_single_indir(fs, dindir_block, index);
		if (index2 == 0) {
			indir_block = ext2_alloc_block(fs);
			uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
			if (!indir_buf)
				return -ENOMEM;

			ext2_read_block(fs, indir_buf, dindir_block);
			indir_buf[index] = indir_block;
			ext2_write_block(fs, indir_buf, dindir_block);
			kfree(indir_buf);
		}

		block = ext2_alloc_block_indir(fs, indir_block, index2);
	} else if (num_blocks < N_DIRECT + N_INDIR + N_DINDIR + N_TINDIR) {
		size_t index = (num_blocks - N_DIRECT - N_INDIR - N_DINDIR) / (N_PER_INDIRECT * N_PER_INDIRECT);
		size_t index2 = ((num_blocks - N_DIRECT - N_INDIR - N_DINDIR) % (N_PER_INDIRECT * N_PER_INDIRECT)) / N_PER_INDIRECT;
		size_t index3 = ((num_blocks - N_DIRECT - N_INDIR - N_DINDIR) % (N_PER_INDIRECT * N_PER_INDIRECT)) % N_PER_INDIRECT;

		long tindir_block = inode->block[N_DIRECT + 2];
		if (index == 0) {
			tindir_block = ext2_alloc_block(fs);
			inode->block[N_DIRECT + 2] = tindir_block;
		}

		long dindir_block = ext2_block_double_indir(fs, tindir_block, index, index2);
		if (index3 == 0) {
			dindir_block = ext2_alloc_block(fs);
			uint32_t *dindir_buf = kmalloc(fs->block_size, ALLOC_DMA);
			if (!dindir_buf)
				return -ENOMEM;

			ext2_read_block(fs, dindir_buf, tindir_block);
			dindir_buf[index] = dindir_block;
			ext2_write_block(fs, dindir_buf, tindir_block);
			kfree(dindir_buf);
		}

		long indir_block = ext2_block_single_indir(fs, dindir_block, index3);
		if (indir_block == 0) {
			indir_block = ext2_alloc_block(fs);
			uint32_t *indir_buf = kmalloc(fs->block_size, ALLOC_DMA);
			if (!indir_buf)
				return -ENOMEM;

			ext2_read_block(fs, indir_buf, dindir_block);
			indir_buf[index3] = indir_block;
			ext2_write_block(fs, indir_buf, dindir_block);
			kfree(indir_buf);
		}

		block = ext2_alloc_block_indir(fs, indir_block, index3);
	} else {
		return -ENOSPC;
	}

	/* write inode to disk */
	inode->blocks += fs->block_size / fs->bdev->block_size;
	ext2_write_inode(fs, inode, inode_no);

	return block;
}

static int ext2_free_block(struct ext2fs *fs, uint32_t block_no)
{
	/* find the group that this block belongs to */
	uint32_t group = (block_no - 1) / fs->sb.blocks_per_group;
	uint32_t index = (block_no - 1) % fs->sb.blocks_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[group];

	/* mark the block as free */
	char *buf = kmalloc(fs->block_size * fs->bgdt_num_blocks, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	ext2_read_block(fs, buf, bg->block_bitmap);

	uint32_t byte = index / 8;
	uint32_t bit = index % 8;

	uint8_t *byte_ptr = (uint8_t *)buf + byte;
	*byte_ptr &= ~(1 << bit);

	bg->free_blocks_count++;
	fs->sb.free_blocks_count++;

	/* update superblock and bgdt */
	ext2_write_super(fs);
	ext2_write_bgdt(fs);

	ext2_write_block(fs, buf, bg->block_bitmap);

	kfree(buf);

	return 0;
}

static long ext2_alloc_inode(struct fs *vfs, uint32_t mode)
{
	/* allocate an inode */
	struct ext2fs *fs = (struct ext2fs *)vfs->fs;

	uint32_t num_groups = fs->sb.blocks_count / fs->sb.blocks_per_group;
	if (fs->sb.blocks_count % fs->sb.blocks_per_group)
		num_groups++;

	for (int i = 0; i < num_groups; i++) {
		/* iterate free groups for a free inode */
		struct ext2_group_desc *bg = &fs->bgdt[i];

		if (bg->free_inodes_count == 0)
			continue;

		uint8_t *inode_bitmap = kmalloc(fs->block_size, ALLOC_DMA);
		if (!inode_bitmap)
			return -ENOMEM;

		ext2_read_block(fs, inode_bitmap, bg->inode_bitmap);

		/* iterate over the inode bitmap to find a free inode */
		uint32_t byte = 0;
		uint8_t *byte_ptr = inode_bitmap + byte;

		while (*byte_ptr == 0xFF) {
			byte++;
			byte_ptr = inode_bitmap + byte;
		}

		uint32_t bit = __builtin_ffs(~byte) - 1;
		*byte_ptr |= (1 << bit);

		ino_t inode_no = (i * fs->sb.inodes_per_group) + (byte * 8) + bit + 1;

		/* write inode bitmap back to disk */
		ext2_write_block(fs, inode_bitmap, bg->inode_bitmap);

		kfree(inode_bitmap);

		/* update superblock and bgdt */
		fs->sb.free_inodes_count--;

		bg->free_inodes_count--;

		ext2_write_super(fs);
		ext2_write_bgdt(fs);

		/* create the inode */
		struct ext2_inode *inode_table = kmalloc(fs->block_size, ALLOC_DMA);
		if (!inode_table)
			return -ENOMEM;

		ext2_read_block(fs, inode_table, bg->inode_table);
		struct ext2_inode *ino = &inode_table[(inode_no - 1) % fs->sb.inodes_per_group];
		memset(ino, 0, sizeof(*ino));
		ino->mode = mode;

		/* write inode back to disk */
		ext2_write_block(fs, inode_table, bg->inode_table);

		kfree(inode_table);

		return inode_no;
	}

	return -ENOSPC;
}

static int ext2_free_inode(struct fs *vfs, ino_t ino_num)
{
	struct ext2fs *fs = (struct ext2fs *)vfs->fs;

	uint32_t group = (ino_num - 1) / fs->sb.inodes_per_group;
	uint32_t index = (ino_num - 1) % fs->sb.inodes_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[group];

	struct ext2_inode *inode_table = kmalloc(fs->block_size, ALLOC_DMA);
	if (!inode_table)
		return -ENOMEM;

	ext2_read_block(fs, inode_table, bg->inode_table);

	struct ext2_inode *ino = &inode_table[index];

	/* free all the blocks */
	size_t num_blocks = (ino->blocks * fs->bdev->block_size) / fs->block_size;
	for (size_t i = 0; i < num_blocks; i++) {
		long ret = ext2_free_block(fs, ext2_inode_get_block(fs, ino, i));
		if (ret < 0) {
			kfree(inode_table);
			return ret;
		}
	}

	if (num_blocks >= N_DIRECT + N_INDIR)
		ext2_free_block(fs, ino->block[N_DIRECT]);
	if (num_blocks >= N_DIRECT + N_INDIR + N_DINDIR)
		ext2_free_block(fs, ino->block[N_DIRECT + 1]);
	if (num_blocks >= N_DIRECT + N_INDIR + N_DINDIR + N_TINDIR)
		ext2_free_block(fs, ino->block[N_DIRECT + 2]);

	memset(ino, 0, sizeof(*ino));

	ext2_write_block(fs, inode_table, bg->inode_table);

	/* update superblock and bgdt */
	fs->sb.free_inodes_count++;

	bg->free_inodes_count++;

	ext2_write_super(fs);
	ext2_write_bgdt(fs);

	kfree(inode_table);

	return 0;
}

static long ext2_inode_find_by_name(struct ext2fs *fs, const char *path)
{
	struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode), ALLOC_DMA);

	ino_t inode_no = 2;
	int ret = ext2_read_inode(fs, inode, inode_no);
	if (ret < 0) {
		kfree(inode);
		return ret;
	}

	char *path_copy = kzalloc(strlen(path) + 1, ALLOC_KERN);
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

		/* inode blocks value is number of BDEV blocks, not EXT2 blocks. so we convert it */
		size_t ino_num_blocks = (inode->blocks * fs->bdev->block_size) / fs->block_size;
		for (int i = 0; i < ino_num_blocks; i++) {
			/* since this inode is a directory, we just read the blocks
			 * sequentially until we find the file we're looking for
			 */

			ext2_inode_read_block(fs, inode, entry, i);

			uint32_t rec_offset = 0;
			while (entry->inode && rec_offset < fs->block_size) {
				inode_no = entry->inode;
				size_t token_len = strlen(token);
				if (!strncmp(entry->name, token, MIN(entry->name_len, token_len))) {
					if (token_len == entry->name_len || token_len == 0) {
						goto found;
					}
				}

				entry = (void *)entry + entry->rec_len;
				rec_offset += entry->rec_len;
			}
		}

		if (!entry || !entry->inode || ino_num_blocks == 0) {
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
	kfree(oentry);

	return inode_no;
}

static int ext2_readdir(struct vnode *vnode, struct dirent **out)
{
	struct fs *vfs = vnode->fs;
	struct ext2_inode *inode = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
	if (!inode)
		return -ENOMEM;

	struct ext2fs *extfs = (struct ext2fs *)vfs->fs;
	int ret = ext2_read_inode(extfs, inode, vnode->ino_num);
	if (ret < 0) {
		kfree(inode);
		return ret;
	}

	if ((inode->mode & 0xF000) != EXT2_INO_DIR) {
		kfree(inode);
		return -ENOTDIR;
	}

	struct ext2_dir_entry *entry = kmalloc(extfs->block_size, ALLOC_DMA);
	void *oentry = entry;
	if (!entry) {
		kfree(inode);
		return -ENOMEM;
	}

	/* populate the dirent array with all the entries in the inode */
	int num_entries = 0;
	size_t ino_num_blocks = (inode->blocks * extfs->bdev->block_size) / extfs->block_size;
	size_t dir_arr_size = ino_num_blocks * extfs->block_size;
	dir_arr_size /= sizeof(struct dirent);

	struct dirent *dir_arr = kmalloc(dir_arr_size * sizeof(struct dirent), ALLOC_KERN);
	if (!dir_arr) {
		kfree(inode);
		kfree(entry);
		return -ENOMEM;
	}

	for (int i = 0; i < ino_num_blocks; i++) {
		ext2_inode_read_block(extfs, inode, entry, i);

		uint32_t rec_offset = 0;
		while (rec_offset < extfs->block_size) {
			if (entry->inode == 0) {
				rec_offset += 1;
				continue;
			}

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

	/* copy to a fixed-size array */
	struct dirent *dirents = kmalloc(num_entries * sizeof(struct dirent), ALLOC_KERN);
	for (int i = 0; i < num_entries; i++) {
		dirents[i] = dir_arr[i];
	}
	kfree(dir_arr);

	kfree(inode);
	kfree(oentry);

	*out = dirents;
	return num_entries;
}

static int ext2_write_file(struct vnode *vnode, void *buf, size_t offset, size_t count)
{
	struct fs *vfs = vnode->fs;
	struct ext2fs *fs = vfs->fs;
	struct ext2_inode inode;

	int res = ext2_read_inode(fs, &inode, vnode->ino_num);
	if (res < 0)
		return res;

	if ((inode.flags & 0xF000) != EXT2_INO_REG)
		return -EISDIR;

	size_t cur_num_blocks = (inode.blocks * fs->block_size) / fs->bdev->block_size;
	size_t block = offset / fs->block_size;
	size_t ooffset = offset;

	if (offset > vnode->size)
		return -EINVAL;

	if (offset + count > vnode->size) {
		/* allocate more blocks */
		size_t new_num_blocks = npow2(offset + count) / fs->block_size;
		new_num_blocks -= MIN(cur_num_blocks, new_num_blocks);

		for (size_t i = 0; i < new_num_blocks; i++) {
			long block_no = ext2_inode_alloc_block(fs, (struct ext2_inode *)&inode, vnode->ino_num);
			if (block_no < 0)
				return block_no;
		}
	}

	offset %= fs->block_size;
	void *block_buf = kmalloc(fs->block_size, ALLOC_KERN);

	if (!block_buf)
		return -ENOMEM;

	size_t bufoffset = 0;
	size_t num = 0;

	while (count > 0) {
		int retval = ext2_inode_read_block(fs, (struct ext2_inode *)&inode, block_buf, block);
		if (retval < 0) {
			kfree(block_buf);
			return retval;
		}

		memcpy(block_buf + offset, buf + bufoffset, MIN(count, fs->block_size - offset));
		bufoffset += fs->block_size;
		offset = 0;

		retval = ext2_inode_write_block(fs, (struct ext2_inode *)&inode, block_buf, block);
		if (retval < 0) {
			kfree(block_buf);
			return retval;
		}

		/* adjust count accounting for the fact that we may have written less than a block */
		num += MIN(fs->block_size - offset, count);
		count -= MIN(fs->block_size - offset, count);
		offset = 0;
		block++;
	}

	kfree(block_buf);

	inode.size = MAX(vnode->size, num + ooffset);
	ext2_write_inode(fs, (struct ext2_inode *)&inode, vnode->ino_num);

	return num;
}

static int ino_type_to_dent(uint32_t ino_flags)
{
	/* convert from ext2 inode type to dirent type */
	switch (ino_flags & 0xF000) {
	case EXT2_INO_REG:
		return EXT2_DE_FILE;
	case EXT2_INO_DIR:
		return EXT2_DE_DIR;
	case EXT2_INO_CHARDEV:
		return EXT2_DE_CHRDEV;
	case EXT2_INO_BLKDEV:
		return EXT2_DE_BLKDEV;
	case EXT2_INO_FIFO:
		return EXT2_DE_FIFO;
	case EXT2_INO_SOCK:
		return EXT2_DE_SOCK;
	case EXT2_INO_SYMLINK:
		return EXT2_DE_SYMLINK;
	}

	return -1;
}

static int ext2_unlink(struct fs *vfs, const char *path)
{
	struct ext2fs *fs = (struct ext2fs *)vfs->fs;

	char *path_copy = kzalloc(strlen(path) + 1, ALLOC_KERN);
	if (!path_copy)
		return -ENOMEM;

	strcpy(path_copy, path);

	char *base = basename(path_copy);
	char *dir = dirname(path_copy);

	long parent_ino_num;
	if (dir == NULL)
		parent_ino_num = EXT2_ROOT_INO;
	else
		parent_ino_num = ext2_inode_find_by_name(vfs->fs, dir);

	if (parent_ino_num < 0) {
		kfree(path_copy);
		return parent_ino_num;
	}

	struct ext2_inode *dir_inode = kmalloc(sizeof(struct ext2_inode), ALLOC_DMA);
	if (!dir_inode) {
		kfree(path_copy);
		return -ENOMEM;
	}

	ext2_read_inode(vfs->fs, dir_inode, parent_ino_num);

	struct ext2_dir_entry *entry = kmalloc(fs->block_size, ALLOC_DMA);
	if (!entry) {
		kfree(path_copy);
		kfree(dir_inode);
		return -ENOMEM;
	}

	size_t baselen = strlen(base);
	size_t num_blocks = (dir_inode->blocks * fs->bdev->block_size) / fs->block_size;

	for (size_t i = 0; i < num_blocks; i++) {
		ext2_inode_read_block(fs, dir_inode, entry, i);

		uint32_t rec_offset = 0;
		uint32_t last_len = 0;
		while (rec_offset < fs->block_size) {
			struct ext2_dir_entry *d_ent = (void *)entry + rec_offset;

			if (d_ent->name_len == baselen && !strncmp(d_ent->name, base, strlen(base))) {
				/* free the inode */
				struct ext2_inode inode;
				ext2_read_inode(fs, &inode, d_ent->inode);

				if (inode.links_count == 1) {
					ext2_free_inode(vfs, d_ent->inode);
				} else {
					inode.links_count--;
					ext2_write_inode(fs, &inode, d_ent->inode);
				}

				/* if this entry is not the first entry, we add its rec_len to the previous entry */
				if (last_len) {
					struct ext2_dir_entry *last_ent = (void *)entry + rec_offset - last_len;
					last_ent->rec_len += d_ent->rec_len;
				}

				/* remove the entry */
				memset(d_ent, 0, d_ent->rec_len);

				/* write the block back to disk */
				ext2_inode_write_block(fs, dir_inode, entry, i);

				kfree(path_copy);
				kfree(dir_inode);
				kfree(entry);

				return 0;
			}

			last_len = d_ent->rec_len;
			rec_offset += d_ent->rec_len;
		}
	}

	kfree(path_copy);
	kfree(dir_inode);
	kfree(entry);

	return -ENOENT;
}

static int ext2_read_file(struct vnode *vnode, void *buf, size_t offset, size_t count)
{
	struct fs *vfs = vnode->fs;
	struct ext2fs *fs = vfs->fs;
	struct ext2_inode inode;

	int res = ext2_read_inode(fs, &inode, vnode->ino_num);
	if (res < 0)
		return res;

	if ((inode.mode & 0xF000) != EXT2_INO_REG)
		return -EISDIR;

	size_t block = offset / fs->block_size;

	if (offset > vnode->size)
		return -EINVAL;

	if (offset + count > vnode->size)
		count = (vnode->size - offset);

	offset %= fs->block_size;

	size_t ret = count;

	void *block_buf = kmalloc(fs->block_size, ALLOC_KERN);

	if (!block_buf)
		return -ENOMEM;

	size_t bufoffset = 0;

	while (count > 0) {
		int retval = ext2_inode_read_block(fs, (struct ext2_inode *)&inode, block_buf, block);
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

static int ext2_open_vno(struct fs *vfs, struct vnode *out, ino_t ino_num)
{
	struct ext2fs *fs = (struct ext2fs *)vfs->fs;
	struct ext2_inode inode;

	int res = ext2_read_inode(fs, &inode, ino_num);
	if (res < 0)
		return res;

	out->flags = inode.mode;
	out->uid = inode.uid;
	out->gid = inode.gid;
	out->size = inode.size;
	out->ino_num = ino_num;

	out->fs = vfs;

	/* Init to NULL, allow vfs to fill in */
	out->ptr = NULL;
	out->next = NULL;
	out->dirents = NULL;

	return res;
}

static int ext2_open_dir(struct fs *vfs, struct vnode *vnode)
{
	/* read dirents from the inode */
	if ((vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_DIR)
		return -ENOTDIR;

	struct dirent *dirents;
	ssize_t num_entries = ext2_readdir(vnode, &dirents);
	if (num_entries < 0)
		return num_entries;

	vnode->dirents = dirents;
	vnode->num_dirents = num_entries;

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
	extfs->bgdt_blockno = bgdt_block;
	extfs->bgdt_num_blocks = npow2(bgdt_size) / extfs->block_size;
	memcpy(extfs->bgdt, buf, bgdt_size);

	kfree(buf);

	struct fs *ret = vfs_create();
	ret->fs = extfs;
	ret->type = VFS_TYPE_EXT2;

	ext2_ops = kmalloc(sizeof(struct fs_ops), ALLOC_KERN);

	if (!ext2_ops)
		goto out;

	ext2_ops->open_vno = ext2_open_vno;
	ext2_ops->open_dir = ext2_open_dir;
	ext2_ops->read = ext2_read_file;
	ext2_ops->write = ext2_write_file;
	ext2_ops->readdir = ext2_readdir;
	ext2_ops->unlink = ext2_unlink;

	ret->ops = ext2_ops;

	struct ext2_inode *root_inode = kmalloc(sizeof(struct ext2_inode), ALLOC_DMA);
	if (!root_inode)
		goto out_2;

	ext2_read_inode(ret->fs, root_inode, EXT2_ROOT_INO);

	ret->root = vfs_create_vno();
	if (!ret->root)
		goto out_3;

	ret->root->name[0] = '/';
	ret->root->fs = ret;
	ret->root->size = root_inode->size;
	ret->root->uid = root_inode->uid;
	ret->root->gid = root_inode->gid;
	ret->root->flags = root_inode->mode;
	ret->root->ino_num = EXT2_ROOT_INO;
	ret->root->no_free = true;

	return ret;

out_3:
	kfree(ret);
	kfree(root_inode);
out_2:
	kfree(ext2_ops);
out:
	kfree(extfs->bgdt);
	kfree(extfs);

	return NULL;
}
