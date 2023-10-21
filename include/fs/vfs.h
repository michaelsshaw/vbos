/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/common.h>

#define VFSE_IS_BDEV 0xFFFA

#define VFS_TYPE_EXT2 0x0001

#define VFS_FILE_FILE 0x0001
#define VFS_FILE_DIR 0x0002
#define VFS_FILE_CHARDEV 0x0003
#define VFS_FILE_BLKDEV 0x0004
#define VFS_FILE_FIFO 0x0005
#define VFS_FILE_SOCK 0x0006
#define VFS_FILE_SYMLINK 0x0007

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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

struct dirent {
	uint64_t inode;
	uint16_t reclen;
	uint8_t type;
	char name[256];
};

typedef struct _DIR {
	struct fs *fs;
	struct dirent *dirents;
	size_t num_dirents;
	struct file *file;
	int fd;
	uint64_t pos;
} DIR;

struct file {
	struct inode inode;

	uint32_t inode_num; /* inode number */
	uint32_t type; /* type of the file */
	uint64_t size; /* size of the file */

	char *path; /* path to the file */

	void *fs_file; /* filesystem specific object */
};

struct file_descriptor {
	struct file file;

	uint64_t pos;
	int fd;
	uint32_t flags;
	uint32_t mode;

	struct fs *fs;
};

struct fs_ops {
	int (*read)(struct fs *fs, struct file *file, void *buf, size_t offset, size_t size);
	int (*write)(struct fs *fs, struct file *file, void *buf, size_t offset, size_t size);
	int (*open)(struct fs *fs, struct file *file, const char *path);
	int (*close)(struct fs *fs, struct file *file);
	int (*readdir)(struct fs *fs, uint32_t ino, struct dirent **dir);
};

struct fs {
	void *fs;
	uint32_t type;
	struct fs_ops ops;

	char *mount_point;
};

void vfs_init(const char *rootfs);
int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);
int open(const char *pathname, int flags);
int close(int fd);
int seek(int fd, size_t offset, int whence);

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif /* _VFS_H_ */
