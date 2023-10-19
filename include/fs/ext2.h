/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _EXT2_H_
#define _EXT2_H_

#include <kernel/common.h>
#include <kernel/block.h>
#include <kernel/gpt.h>

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_ROOT_INO 2
#define EXT2_SUPERBLOCK_BLOCKNO 1

#define EXT2_DE_UNKNOWN 0
#define EXT2_DE_FILE 1
#define EXT2_DE_DIR 2
#define EXT2_DE_CHRDEV 3
#define EXT2_DE_BLKDEV 4
#define EXT2_DE_FIFO 5
#define EXT2_DE_SOCK 6
#define EXT2_DE_SYMLINK 7

#define EXT2_FLAG_LONG_FILESIZE 0x0002

struct ext2_superblock {
	uint32_t inodes_count;
	uint32_t blocks_count;
	uint32_t u_blocks_count;
	uint32_t free_blocks_count;
	uint32_t free_inodes_count;
	uint32_t first_data_block;

	uint32_t log_block_size;
	uint32_t log_frag_size;

	uint32_t blocks_per_group;
	uint32_t frags_per_group;
	uint32_t inodes_per_group;

	uint32_t mtime;
	uint32_t wtime;

	uint16_t mnt_count;
	uint16_t max_mnt_count;
	uint16_t magic;
	uint16_t state;
	uint16_t errors;
	uint16_t minor_rev_level;

	uint32_t lastcheck;
	uint32_t checkinterval;
	uint32_t creator_os;
	uint32_t rev_level;

	uint16_t def_resuid;
	uint16_t def_resgid;

	/* extended superblock fields 
	 * if rev_level >= 1 
	 */
	uint32_t first_ino;
	uint16_t inode_size;
	uint16_t block_group_nr;
	uint32_t feature_compat;
	uint32_t feature_incompat;
	uint32_t feature_ro_compat;
	uint8_t uuid[16];
	char volume_name[16];
	char last_mounted[64];
	uint32_t algo_bitmap;

	/* performance hints */
	uint8_t prealloc_blocks;
	uint8_t prealloc_dir_blocks;
	uint16_t padding1;

	/* journaling support */
	uint8_t journal_uuid[16];
	uint32_t journal_inum;
	uint32_t journal_dev;
	uint32_t last_orphan;
} PACKED;

struct ext2_inode {
	uint16_t mode;
	uint16_t uid;
	uint32_t size;

	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;

	uint16_t gid;
	uint16_t links_count;
	uint32_t blocks;
	uint32_t flags;
	uint32_t osd1;
	uint32_t block[12];

	uint32_t block_indirect;
	uint32_t block_dindirect;
	uint32_t block_tindirect;

	uint32_t generation;
	uint32_t file_acl;
	uint32_t dir_acl;
	uint32_t faddr;

	uint8_t frag_number;
	uint8_t frag_size;
	uint16_t padding1;
	uint16_t user_high;
	uint16_t group_high;
	uint32_t padding2;
} PACKED;

struct ext2_group_desc {
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
	uint16_t free_blocks_count;
	uint16_t free_inodes_count;
	uint16_t used_dirs_count;
	uint16_t padding[7];
} PACKED;

struct ext2_dir_entry {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
	char name[];
} PACKED;

struct ext2fs {
	struct ext2_superblock sb;
	struct block_device *bdev;

	struct ext2_group_desc *bgdt;

	uint32_t block_size;
};

struct ext2fs *ext2_init_fs(struct block_device *bdev);

/* Filesystem block sizes are not equal to block device block sizes */
int ext2_read_block(struct ext2fs *fs, void *buf, size_t block);
int ext2_write_block(struct ext2fs *fs, void *buf, size_t block);
int ext2_read_inode(struct ext2fs *fs, struct ext2_inode *out, uint32_t inode);
int ext2_read_inode(struct ext2fs *fs, struct ext2_inode *in, uint32_t inode);

#endif /* _EXT2_H_ */
