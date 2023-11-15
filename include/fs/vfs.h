/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/common.h>

#define VFSE_IS_BDEV 0xFFFA

#define VFS_TYPE_EXT2 0x0001

#define VFS_VNO_FIFO 0x1000
#define VFS_VNO_CHARDEV 0x2000
#define VFS_VNO_DIR 0x4000
#define VFS_VNO_BLKDEV 0x6000
#define VFS_VNO_REG 0x8000
#define VFS_VNO_SYMLINK 0xA000
#define VFS_VNO_SOCK 0xC000
#define VFS_VTYPE_MASK 0xF000

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0004
#define O_CREAT 0x0008
#define O_TRUNC 0x0010
#define O_APPEND 0x0020

typedef uint32_t ino_t;
typedef uint8_t ftype_t;

struct vnode {
	char name[256];
	uint32_t flags;
	uint32_t gid;
	uint32_t uid;
	uint32_t size;
	uint32_t ino_num;

	struct fs *fs;
};

struct dirent {
	uint64_t inode;
	uint16_t reclen;
	ftype_t type;
	char name[256];
};

typedef struct _DIR {
	struct fs *fs;
	struct dirent *dirents;
	size_t num_dirents;
	struct vnode *vnode;
	int fd;
	uint64_t pos;
} DIR;

struct file {
	struct vnode vnode;

	ino_t inode_num; /* inode number */
	uint32_t type; /* type of the file */
	uint64_t size; /* size of the file */

	void *fs_file; /* filesystem specific object */
};

struct file_descriptor {
	struct vnode vnode;

	uint64_t pos;
	int fd;
	uint32_t flags;
	uint32_t mode;

	struct fs *fs;
};

struct fs_ops {
	int (*read)(struct fs *fs, struct vnode *vnode, void *buf, size_t offset, size_t size);
	int (*write)(struct fs *fs, struct vnode *vnode, void *buf, size_t offset, size_t size);
	int (*open)(struct fs *fs, struct vnode *vnode, const char *path);
	int (*readdir)(struct fs *fs, uint32_t ino, struct dirent **dir);
	int (*mkdir)(struct fs *fs, const char *path);
	int (*unlink)(struct fs *fs, const char *path);
};

struct fs {
	void *fs;
	uint32_t type;
	struct fs_ops *ops;

	char *mount_point;
};

struct statbuf {
	size_t size;
};

void vfs_init(const char *rootfs);
int write(int fd, void *buf, size_t count);
int read(int fd, void *buf, size_t count);
int open(const char *pathname, int flags);
int close(int fd);
int seek(int fd, size_t offset, int whence);
size_t tell(int fd);
int unlink(const char *pathname);
int statfd(int fd, struct statbuf *statbuf);
int mkdir(const char *pathname);

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

char *basename(char *path);
char *dirname(char *path);

#endif /* _VFS_H_ */
