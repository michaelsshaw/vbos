/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/common.h>

#define VFS_TYPE_EXT2 0x0001

struct inode {
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
} PACKED; /* PACKED to allow for memcpy */

struct file {
	struct inode inode;

	uint32_t inode_num; /* inode number */
	uint64_t size; /* size of the file */
	uint64_t pos; /* current position in the file */

	void *fs_file;
};

struct fs_ops {
	int (*read)(void *fs, struct file *file, void *buf, size_t size);
	int (*write)(void *fs, struct file *file, void *buf, size_t size);
	int (*open)(void *fs, struct file *file, const char *path);
	int (*close)(void *fs, struct file *file);
	int (*seek)(void *fs, struct file *file, size_t offset);
};

struct fs {
	void *fs;
	uint32_t type;
	struct fs_ops ops;
};

void vfs_init(const char *rootfs);

#endif /* _VFS_H_ */
