/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/lock.h>
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

struct ext2_block_index {
	uint32_t n_indirs;
	uint32_t indices[4];
};

static struct fs_ops *ext2_ops;

static uint8_t inode_to_ftype[] = {
	ITYPE_DECL(EXT2_INO_FIFO, EXT2_DE_FIFO), ITYPE_DECL(EXT2_INO_CHARDEV, EXT2_DE_CHRDEV),
	ITYPE_DECL(EXT2_INO_DIR, EXT2_DE_DIR),	 ITYPE_DECL(EXT2_INO_BLKDEV, EXT2_DE_BLKDEV),
	ITYPE_DECL(EXT2_INO_REG, EXT2_DE_FILE),	 ITYPE_DECL(EXT2_INO_SYMLINK, EXT2_DE_SYMLINK),
	ITYPE_DECL(EXT2_INO_SOCK, EXT2_DE_SOCK),
};

#undef ITYPE_DECL
#undef DTYPE_DECL

/* Base ext2 operations:
 * -----------------------------------------------------------------------------
 * Superblock
 * R/W block
 * BGDT_write
 * -----------------------------------------------------------------------------
 */
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

static int ext2_write_block(struct ext2fs *fs, void *buf, size_t block)
{
	uint32_t bdev_block_size = fs->bdev->block_size;
	uint32_t num_blocks_read = fs->block_size / bdev_block_size;

	uint32_t bdev_block = block * num_blocks_read;

	return block_write(fs->bdev, buf, bdev_block, num_blocks_read);
}

static int ext2_write_super(struct ext2fs *fs)
{
	return ext2_write_block(fs, &fs->sb, 1);
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

static long ext2_find_free_block(struct ext2fs *fs, size_t block_group)
{
	struct ext2_group_desc *bg = &fs->bgdt[block_group];

	if (!bg->free_blocks_count)
		return -ENOSPC;

	uint32_t block = bg->block_bitmap;
	uint32_t block_offset = 0;

	char *buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	ext2_read_block(fs, buf, block);

	uint8_t *byte;
	while (block_offset < fs->block_size) {
		byte = (uint8_t *)buf + block_offset;
		if (*byte == 0xFF) {
			block_offset += 8;
		} else {
			break;
		}
	}
	int j;
	for (j = 0; j < 8; j++) {
		if (!(*byte & (1 << j)))
			break;
	}

	*byte |= (1 << j);
	bg->free_blocks_count--;
	fs->sb.free_blocks_count--;

	ext2_write_block(fs, buf, block);

	/* update superblock and bgdt */
	ext2_write_super(fs);
	ext2_write_bgdt(fs);

	kfree(buf);

	uint32_t block_ret_no = (block_group * fs->sb.blocks_per_group) + (block_offset * 8) + j + 1;

	/* zero out the block */
	char *block_buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!block_buf)
		return -ENOMEM;

	memset(block_buf, 0, fs->block_size);
	ext2_write_block(fs, block_buf, block_ret_no);
	kfree(block_buf);

	return block_ret_no;
}

static long ext2_alloc_block(struct ext2fs *fs)
{
	uint32_t num_groups = fs->sb.blocks_count / fs->sb.blocks_per_group;
	if (fs->sb.blocks_count % fs->sb.blocks_per_group)
		num_groups++;

	spinlock_acquire(&fs->lock);
	for (int i = 0; i < num_groups; i++) {
		long ret = ext2_find_free_block(fs, i);
		if (ret >= 0)
			return ret;
	}
	spinlock_release(&fs->lock);

	return -ENOSPC;
}

static long ext2_free_block(struct ext2fs *fs, uint32_t block)
{
	uint32_t block_group = (block - 1) / fs->sb.blocks_per_group;
	uint32_t block_offset = (block - 1) % fs->sb.blocks_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[block_group];

	uint32_t block_bitmap_block = bg->block_bitmap;
	uint32_t block_bitmap_offset = block_offset >> 3;

	char *buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	spinlock_acquire(&fs->lock);
	ext2_read_block(fs, buf, block_bitmap_block);

	uint8_t *byte = (uint8_t *)buf + block_bitmap_offset;
	*byte &= ~(1 << (block_offset & 7));
	bg->free_blocks_count++;
	fs->sb.free_blocks_count++;
	ext2_write_block(fs, buf, block_bitmap_block);

	ext2_write_super(fs);
	ext2_write_bgdt(fs);

	spinlock_release(&fs->lock);

	kfree(buf);
	return 0;
}

/* inode operations:
 * -----------------------------------------------------------------------------
 * alloc inode
 * free inode
 * R/W inode 
 * Get block index (helper)
 * Get block number (helper)
 *
 * inode block operations:
 * ext2_ino_read_block
 * ext2_ino_write_block
 * 
 * ext2_dno_find (helper)
 * ext2_ino_find_by_name: search for a file by name
 * -----------------------------------------------------------------------------
 */
static long ext2_alloc_inode(struct ext2fs *fs, struct ext2_inode *out)
{
	uint32_t num_groups = fs->sb.blocks_count / fs->sb.blocks_per_group;
	if (fs->sb.blocks_count % fs->sb.blocks_per_group)
		num_groups++;

	for (size_t i = 0; i < num_groups; i++) {
		struct ext2_group_desc *bg = &fs->bgdt[i];

		if (!bg->free_inodes_count)
			continue;

		uint32_t block = bg->inode_bitmap;
		uint32_t block_offset = 0;

		char *buf = kmalloc(fs->block_size, ALLOC_DMA);
		if (!buf)
			return -ENOMEM;

		ext2_read_block(fs, buf, block);

		spinlock_acquire(&fs->lock);
		uint8_t *byte;
		while (block_offset < fs->block_size) {
			byte = (uint8_t *)buf + block_offset;
			if (*byte == 0xFF)
				block_offset += 8;
			else
				break;
		}
		int j;
		for (j = 0; j < 8; j++) {
			if (!(*byte & (1 << j)))
				break;
		}

		*byte |= (1 << j);
		bg->free_inodes_count--;
		fs->sb.free_inodes_count--;

		ext2_write_block(fs, buf, block);

		/* update superblock and bgdt */
		ext2_write_super(fs);
		ext2_write_bgdt(fs);

		kfree(buf);
		spinlock_release(&fs->lock);

		uint32_t inode_no = (i * fs->sb.inodes_per_group) + (block_offset << 3) + j + 1;
		return inode_no;
	}

	return -ENOSPC;
}

static int ext2_free_inode(struct ext2fs *fs, ino_t inode)
{
	uint32_t group = (inode - 1) / fs->sb.inodes_per_group;
	uint32_t index = (inode - 1) % fs->sb.inodes_per_group;

	struct ext2_group_desc *bg = &fs->bgdt[group];

	uint32_t block = bg->inode_bitmap;
	uint32_t offset = index >> 3;

	char *buf = kmalloc(fs->block_size, ALLOC_DMA);
	if (!buf)
		return -ENOMEM;

	ext2_read_block(fs, buf, block);

	spinlock_acquire(&fs->lock);
	uint8_t *byte = (uint8_t *)buf + offset;
	*byte &= ~(1 << (index & 7));
	bg->free_inodes_count++;
	fs->sb.free_inodes_count++;
	ext2_write_block(fs, buf, block);

	ext2_write_super(fs);
	ext2_write_bgdt(fs);

	spinlock_release(&fs->lock);

	kfree(buf);
	return 0;
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

static void ext2_get_block_idx(struct ext2fs *fs, struct ext2_block_index *ret, uint32_t block)
{
	const uint32_t limit_direct = N_DIRECT;
	const uint32_t limit_indirect = limit_direct + N_INDIR;
	const uint32_t limit_dindirect = limit_indirect + N_DINDIR;
	const uint32_t limit = limit_dindirect + N_TINDIR;

	if (block < N_DIRECT) {
		ret->n_indirs = 0;
		ret->indices[0] = block;
	} else if (block < limit_indirect) {
		ret->n_indirs = 1;
		ret->indices[0] = N_DIRECT;
		ret->indices[1] = block - N_DIRECT;
	} else if (block < limit_dindirect) {
		ret->n_indirs = 2;
		ret->indices[0] = N_DIRECT + 1;
		ret->indices[1] = (block - (N_DIRECT)) / N_PER_INDIRECT;
		ret->indices[2] = (block - (N_DIRECT)) % N_PER_INDIRECT;
	} else if (block < limit) {
		ret->n_indirs = 3;
		/* tindir block */
		ret->indices[0] = N_DIRECT + 2;
		ret->indices[1] = (block - (N_DIRECT + N_DINDIR)) / N_PER_INDIRECT / N_PER_INDIRECT;
		ret->indices[2] = (block - (N_DIRECT + N_DINDIR)) / N_PER_INDIRECT % N_PER_INDIRECT;
		ret->indices[3] = (block - (N_DIRECT + N_DINDIR)) % N_PER_INDIRECT;
	} else {
		ret->n_indirs = -1;
	}
}

static long ext2_ino_get_blocknum(struct ext2fs *fs, struct ext2_inode *ino, uint32_t ino_blocknum, bool alloc)
{
	const uint32_t limit_direct = N_DIRECT;
	const uint32_t limit_indirect = limit_direct + N_INDIR;
	const uint32_t limit_dindirect = limit_indirect + N_DINDIR;
	const uint32_t limit = limit_dindirect + N_TINDIR;

	struct ext2_block_index idx;
	ext2_get_block_idx(fs, &idx, ino_blocknum);

	if (idx.n_indirs == -1)
		return -EINVAL;

	long ret;
	long last = 0;

	ret = ino->block[idx.indices[0]];
	if (ret == 0) {
		if (!alloc)
			return -ENOENT;

		ret = ext2_alloc_block(fs);
		if (ret < 0)
			return ret;

		ino->block[idx.indices[0]] = ret;
	}

	for (int i = 1; i <= idx.n_indirs; i++) {
		last = ret;
		uint32_t *buf = kmalloc(fs->block_size, ALLOC_DMA);
		if (!buf)
			return -ENOMEM;

		ext2_read_block(fs, buf, last);

		ret = buf[idx.indices[i]];

		if (ret == 0) {
			if (!alloc) {
				kfree(buf);
				return -ENOENT;
			}

			ret = ext2_alloc_block(fs);
			if (ret < 0) {
				kfree(buf);
				return ret;
			}

			buf[idx.indices[i]] = ret;
			ext2_write_block(fs, buf, last);

			kfree(buf);
		}
	}

	return ret;
}

static long ext2_ino_read_block(struct ext2fs *fs, struct ext2_inode *ino, void *buf, uint32_t ino_blocknum)
{
	long ret = ext2_ino_get_blocknum(fs, ino, ino_blocknum, false);
	if (ret < 0)
		return ret;

	ext2_read_block(fs, buf, ret);
	return 0;
}

static long ext2_ino_write_block(struct ext2fs *fs, struct ext2_inode *ino, void *buf, uint32_t ino_blocknum)
{
	long ret = ext2_ino_get_blocknum(fs, ino, ino_blocknum, true);
	if (ret < 0)
		return ret;

	ext2_write_block(fs, buf, ret);
	return 0;
}

static long ext2_dno_find(struct ext2fs *fs, struct ext2_inode *ino, char *name)
{
	size_t n_blocks = ino->size;
	if (n_blocks % fs->block_size)
		n_blocks += fs->block_size;
	n_blocks /= fs->block_size;

	struct ext2_dir_entry *entry = kmalloc(fs->block_size * n_blocks, ALLOC_DMA);
	char *entry_buf = (char *)entry;
	if (!entry)
		return -ENOMEM;

	size_t ino_num_blocks = (ino->blocks * fs->bdev->block_size) / fs->block_size;

	uint32_t rec_offset = 0;
	for (int i = 0; i < ino_num_blocks; i++) {
		/* since this ino is a directory, we just read the blocks
			 * sequentially until we find the file we're looking for
			 */

		long ret = ext2_ino_read_block(fs, ino, entry_buf + fs->block_size * i, i);

		if (ret < 0) {
			kfree(entry);
			return ret;
		}
	}

	while (entry->inode && rec_offset < fs->block_size * n_blocks) {
		size_t name_len = strlen(name);
		int res = strncmp(entry->name, name, MIN(entry->name_len, name_len));

		if (!res && (name_len == entry->name_len || name_len == 0)) {
			kfree(entry);
			return entry->inode;
		}
	}

	kfree(entry);
	return -ENOENT;
}

static long ext2_ino_find_by_name(struct ext2fs *fs, const char *path)
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

	while (token) {
		/* inode blocks value is number of BDEV blocks, not EXT2 blocks. so we convert it */

		ret = ext2_dno_find(fs, inode, token);
		if (ret < 0) {
			break;
		}

		ext2_read_inode(fs, inode, ret);
		token = strtok(NULL, "/", &strtok_last);
	}

	kfree(path_copy);
	kfree(inode);

	return ret;
}

static int ext2_ino_add_dirent(struct ext2fs *fs, struct ext2_inode *ino, const char *name, ino_t ino_num, uint8_t type)
{
	size_t n_blocks = ino->size;
	if (n_blocks % fs->block_size)
		n_blocks += fs->block_size;
	n_blocks /= fs->block_size;

	struct ext2_dir_entry *entry = kzalloc(fs->block_size * n_blocks, ALLOC_DMA);
	if (!entry)
		return -ENOMEM;

	size_t this_len = sizeof(struct ext2_dir_entry) + strlen(name);

	/* read all entries */
	size_t offset = 0;
	for (int i = 0; i < n_blocks; i++) {
		long ret = ext2_ino_read_block(fs, ino, entry + offset, i);
		if (ret < 0) {
			kfree(entry);
			return ret;
		}

		offset += fs->block_size;
	}

	offset = 0;
	struct ext2_dir_entry *cur = entry;
	while (cur->inode && offset < fs->block_size * n_blocks) {
		cur = (struct ext2_dir_entry *)((char *)entry + offset);
		if (!cur->inode && cur->rec_len <= this_len)
			break;
		offset += cur->rec_len;
	}

	if (offset + strlen(name) + sizeof(struct ext2_dir_entry) >= fs->block_size * n_blocks) {
		/* easier to do it this way */
		entry = krealloc(entry, (n_blocks + 1) * fs->block_size, ALLOC_DMA);
		memset(entry + n_blocks * fs->block_size, 0, fs->block_size);
		/* this adds a new block */
		ext2_ino_write_block(fs, ino, ((char *)entry) + n_blocks, n_blocks);
		ino->size += fs->block_size;
		n_blocks++;
	}

	cur->inode = ino_num;
	cur->name_len = strlen(name);
	cur->rec_len = fs->block_size - offset;
	cur->file_type = type;
	memcpy(cur->name, name, cur->name_len);

	uint32_t starting_block = offset / fs->block_size;
	for (int i = starting_block; i < n_blocks; i++) {
		ext2_ino_write_block(fs, ino, ((char *)entry) + i * fs->block_size, i);
	}

	kfree(entry);

	return 0;
}

static int ext2_ino_del_dirent(struct ext2fs *fs, struct ext2_inode *ino, const char *name)
{
	size_t n_blocks = ino->size;
	if (n_blocks % fs->block_size)
		n_blocks += fs->block_size;
	n_blocks /= fs->block_size;

	struct ext2_dir_entry *entry = kmalloc(fs->block_size * n_blocks, ALLOC_DMA);
	if (!entry)
		return -ENOMEM;

	/* read all entries */

	size_t offset = 0;
	for (int i = 0; i < n_blocks; i++) {
		long ret = ext2_ino_read_block(fs, ino, entry + offset, i);
		if (ret < 0) {
			kfree(entry);
			return ret;
		}

		offset += fs->block_size;
	}

	/* find the dirent */
	offset = 0;
	struct ext2_dir_entry *cur = NULL;
	struct ext2_dir_entry *prev = NULL;

	spinlock_acquire(&fs->lock);
	while (cur && offset < fs->block_size * n_blocks && cur->inode) {
		cur = (struct ext2_dir_entry *)((char *)entry + offset);

		/* check if the name matches */
		if (!strncmp(cur->name, name, cur->name_len)) {
			/* remove the dirent */
			cur->inode = 0;
			cur->name_len = 0;
			cur->file_type = 0;
			memset(cur->name, 0, cur->name_len);

			/* update the previous dirent */
			if (prev) {
				/* if there is a previous dirent, we need to update the rec_len */
				prev->rec_len += cur->rec_len;
				cur->rec_len = 0;
			} else {
				/* if there is no previous dirent, we need to update the first dirent */
				struct ext2_dir_entry *next = (struct ext2_dir_entry *)((char *)cur + cur->rec_len);
				next->rec_len += cur->rec_len;

				if (next->inode == 0)
					cur->rec_len = n_blocks * fs->block_size - offset;
			}

			/* write all the blocks */
			uint32_t starting_block = offset / fs->block_size;
			for (int i = starting_block; i < n_blocks; i++) 
				ext2_ino_write_block(fs, ino, ((char *)entry) + i * fs->block_size, i);

			spinlock_release(&fs->lock);
			return 0;
		}

		prev = cur;
		offset += cur->rec_len;
	}

	spinlock_release(&fs->lock);
	return -ENOENT;
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

/* VFS operations:
 * -----------------------------------------------------------------------------
 * read dirs (helper)
 * open vno
 * read file
 * write file
 * unlink
 * mkdir
 * creat
 * -----------------------------------------------------------------------------
 */

static long ext2_vno_read_dirs(struct ext2fs *fs, struct ext2_inode *inode, struct vnode *out)
{
	size_t n_blocks = inode->size;
	if (n_blocks % fs->block_size)
		n_blocks += fs->block_size;
	n_blocks /= fs->block_size;

	void *entry_buf = kmalloc(fs->block_size * n_blocks, ALLOC_DMA);
	if (!entry_buf)
		return -ENOMEM;

	/* read the directory entries */
	for (int i = 0; i < n_blocks; i++) {
		long ret = ext2_ino_read_block(fs, inode, entry_buf + fs->block_size * i, i);
		if (ret < 0) {
			kfree(entry_buf);
			return ret;
		}
	}

	void *oe = entry_buf;
	size_t offset = 0;

	while (offset < fs->block_size * n_blocks) {
		struct ext2_dir_entry *entry = (struct ext2_dir_entry *)((char *)entry_buf + offset);
		struct dirent dentry = { 0 };

		if (entry->inode == 0 || entry->name_len == 0) {
			break;
		}

		memcpy(&dentry.name, entry->name, entry->name_len);
		dentry.name[entry->name_len] = '\0';
		dentry.inode = entry->inode;
		dentry.type = entry->file_type;

		/* insert into dirents linked list */
		out->dirents = krealloc(out->dirents, (out->num_dirents + 1) * sizeof(struct dirent), ALLOC_KERN);
		if (!out->dirents) {
			kfree(oe);
			return -ENOMEM;
		}

		memcpy(&out->dirents[out->num_dirents], &dentry, sizeof(struct dirent));

		out->num_dirents++;
		offset += entry->rec_len;
	}

	kfree(entry_buf);

	return 0;
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

	if (inode.mode & EXT2_INO_DIR) {
		/* read the directory entries */
		if ((res = ext2_vno_read_dirs(fs, &inode, out)) < 0)
			return res;
	}

	return 0;
}

static int ext2_read_file(struct vnode *vnode, void *buf, size_t offset, size_t size)
{
	struct fs *fs = vnode->fs;
	struct ext2fs *extfs = (struct ext2fs *)fs->fs;

	struct ext2_inode inode;
	int res = ext2_read_inode(extfs, &inode, vnode->ino_num);
	if (res < 0)
		return res;

	if (offset > inode.size)
		return -EINVAL;

	if (offset + size > inode.size)
		size = inode.size - offset;

	size_t first_block = offset / extfs->block_size;
	size_t last_block = (offset + size) / extfs->block_size;
	size_t n_blocks_read = (last_block - first_block) + 1;

	char *block_buf = kmalloc(n_blocks_read * extfs->block_size, ALLOC_DMA);
	if (!block_buf)
		return -ENOMEM;

	for (size_t i = first_block; i <= last_block; i++) {
		long ret = ext2_ino_read_block(extfs, &inode, block_buf + (i * extfs->block_size), i);
		if (ret < 0) {
			kfree(block_buf);
			return ret;
		}
	}

	memcpy(buf, block_buf + (offset % extfs->block_size), size);
	kfree(block_buf);

	return size;
}

static int ext2_write_file(struct vnode *vnode, void *buf, size_t offset, size_t size)
{
	struct fs *fs = vnode->fs;
	struct ext2fs *extfs = (struct ext2fs *)fs->fs;

	struct ext2_inode inode;
	int res = ext2_read_inode(extfs, &inode, vnode->ino_num);
	if (res < 0)
		return res;

	if (offset + size > inode.size)
		inode.size = offset + size;

	size_t first_block = offset / extfs->block_size;
	size_t last_block = (offset + size) / extfs->block_size;
	size_t n_blocks_read = (last_block - first_block) + 1;

	char *block_buf = kzalloc(n_blocks_read * extfs->block_size, ALLOC_DMA);
	if (!block_buf)
		return -ENOMEM;

	for (size_t i = first_block; i <= last_block; i++) {
		long ret = ext2_ino_read_block(extfs, &inode, block_buf + (i * extfs->block_size), i);
		if (ret < 0 && ret != -ENOENT) {
			kfree(block_buf);
			return ret;
		}
	}

	size_t start_offset = offset % extfs->block_size;
	memcpy(block_buf + start_offset, buf, size);

	for (size_t i = first_block; i <= last_block; i++) {
		long ret = ext2_ino_write_block(extfs, &inode, block_buf + (i * extfs->block_size), i);
		if (ret < 0) {
			kfree(block_buf);
			return ret;
		}
	}

	kfree(block_buf);

	return size;
}

static int ext2_unlink_file(struct vnode *parent, const char *name)
{
	struct fs *fs = parent->fs;
	struct ext2fs *extfs = (struct ext2fs *)fs->fs;

	char *name_cpy = kzalloc(strlen(name) + 1, ALLOC_KERN);
	if (!name_cpy)
		return -ENOMEM;

	strcpy(name_cpy, name);

	struct ext2_inode inode;
	int res = ext2_read_inode(extfs, &inode, parent->ino_num);
	if (res < 0)
		return res;

	res = ext2_ino_del_dirent(extfs, &inode, name_cpy);
	if (res < 0)
		return res;

	return 0;
}

static int ext2_create_file(struct vnode *parent, const char *name, mode_t mode)
{
	struct fs *fs = parent->fs;
	struct ext2fs *extfs = (struct ext2fs *)fs->fs;

	char *name_cpy = kzalloc(strlen(name) + 1, ALLOC_KERN);
	if (!name_cpy)
		return -ENOMEM;

	strcpy(name_cpy, name);

	struct ext2_inode inode;
	int res = ext2_read_inode(extfs, &inode, parent->ino_num);
	if (res < 0)
		return res;

	/* check if file already exists */
	long ret = ext2_dno_find(extfs, &inode, name_cpy);
	if (ret >= 0)
		return -EEXIST;

	/* allocate inode */
	ret = ext2_alloc_inode(extfs, &inode);
	if (ret < 0)
		return ret;

	struct ext2_inode inode_new = { 0 };
	/* notably we do no mode conversion since ext2 is the same as the kernel vfs */
	inode_new.mode = mode;
	inode_new.uid = 0;
	inode_new.gid = 0;
	inode_new.size = 0;
	inode_new.blocks = 0;

	/* write inode */
	res = ext2_write_inode(extfs, &inode_new, ret);

	if (res < 0)
		return res;

	/* write directory entry */
	res = ext2_ino_add_dirent(extfs, &inode, name_cpy, ret, ino_type_to_dent(mode));
	if (res < 0)
		return res;

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
	ext2_ops->read = ext2_read_file;
	ext2_ops->write = ext2_write_file;
	ext2_ops->creat = ext2_create_file;
	ext2_ops->unlink = ext2_unlink_file;

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

	ext2_vno_read_dirs(extfs, root_inode, ret->root);

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
