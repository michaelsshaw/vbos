/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <dev/console.h>

#include <fs/ext2.h>

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

#ifdef KDEBUG
static void ext2_print_dir(struct ext2fs *fs, struct ext2_inode *inode, int indent)
{
	static const char *types[] = { "UNKNOWN", "FILE", "DIR", "CHARDEV", "BLOCKDEV", "FIFO", "SOCKET", "SYMLINK" };
	/* print the names of files and directories in the root directory */

	struct ext2_dir_entry *entry = kmalloc(fs->block_size, ALLOC_DMA);
	void *oentry = entry;
	if (!entry)
		return;

	ext2_read_block(fs, entry, inode->block[0]);

	while (entry->inode) {
		char *name = kmalloc(entry->name_len + 1, ALLOC_KERN);
		if (!name)
			break;

		memcpy(name, entry->name, entry->name_len);
		name[entry->name_len] = '\0';

		kprintf(LOG_DEBUG);
		for (int i = 0; i < indent; i++)
			kprintf("    ");
		kprintf("[%s] %s\n", types[MIN(7, entry->file_type)], name);

		if (entry->file_type == EXT2_DE_DIR) {
			if (!strcmp(name, ".") || !strcmp(name, ".."))
				goto next;

			struct ext2_inode *in = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
			if (!in)
				break;

			ext2_read_inode(fs, in, entry->inode);
			ext2_print_dir(fs, in, indent + 1);
			kfree(in);
		} else if (entry->file_type == EXT2_DE_FILE) {
			/* print the contents of the file */
			struct ext2_inode *in = kmalloc(sizeof(struct ext2_inode), ALLOC_KERN);
			if (!in)
				break;

			ext2_read_inode(fs, in, entry->inode);

			char *buf = kmalloc(in->size, ALLOC_KERN);

			if (!buf) {
				kfree(in);
				break;
			}

			ext2_read_block(fs, buf, in->block[0]);

			kprintf(LOG_DEBUG);
			for (int i = 0; i < indent + 1; i++)
				kprintf("    ");

			for (uint32_t i = 0; i < in->size; i++) {
				if (buf[i] == '\n')
					kprintf("\n");
				else
					console_write(buf[i]);
			}
		}
next:

		kfree(name);

		entry = (void *)entry + entry->rec_len;
	}

	kfree(oentry);
}
#endif /* KDEBUG */

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

	/* read block group descriptor table */
	uint32_t num_groups = ret->sb.blocks_count / ret->sb.blocks_per_group;
	if (ret->sb.blocks_count % ret->sb.blocks_per_group)
		num_groups++;

	uint32_t bgdt_block = 2 + (ret->sb.first_data_block / ret->sb.blocks_per_group);
	uint32_t bgdt_size = num_groups * sizeof(struct ext2_group_desc);

	char *buf = kmalloc(bgdt_size, ALLOC_DMA);

	ext2_read_block(bdev->fs, buf, bgdt_block);

	ret->bgdt = (struct block_group_desc *)kmalloc(bgdt_size, ALLOC_KERN);
	memcpy(ret->bgdt, buf, bgdt_size);

	kfree(buf);

#ifdef KDEBUG
	struct ext2_inode root;
	ext2_read_inode(ret, &root, 2);
	ext2_print_dir(ret, &root, 0);
#endif /* KDEBUG */

	return ret;
}
