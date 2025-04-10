/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _VFS_H_
#define _VFS_H_

#include <kernel/common.h>

#define VFSE_IS_BDEV 0xFFFA

#define VFS_TYPE_EXT2 0x0001

/* same as EXT2_INO_* */
#define VFS_VNO_FIFO 0x1000
#define VFS_VNO_CHARDEV 0x2000
#define VFS_VNO_DIR 0x4000
#define VFS_VNO_BLKDEV 0x6000
#define VFS_VNO_REG 0x8000
#define VFS_VNO_SYMLINK 0xA000
#define VFS_VNO_SOCK 0xC000
#define VFS_VTYPE_MASK 0xF000

#define FTYPE_CHARDEV VFS_VNO_CHARDEV
#define FTYPE_DIR VFS_VNO_DIR
#define FTYPE_BLKDEV VFS_VNO_BLKDEV
#define FTYPE_REG VFS_VNO_REG
#define FTYPE_SYMLINK VFS_VNO_SYMLINK
#define FTYPE_SOCK VFS_VNO_SOCK
#define FTYPE_FIFO VFS_VNO_FIFO

#define VFS_DE_UNKNOWN 0
#define VFS_DE_FILE 1
#define VFS_DE_DIR 2
#define VFS_DE_CHRDEV 3
#define VFS_DE_BLKDEV 4
#define VFS_DE_FIFO 5
#define VFS_DE_SOCK 6
#define VFS_DE_SYMLINK 7

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0004
#define O_CREAT 0x0008
#define O_TRUNC 0x0010
#define O_APPEND 0x0020

#define S_IFIFO 0x1000

#define FS_TYPE_EXT2 0x0001
#define FS_TYPE_PFS 0x0002 /* pseudo fs */

typedef uint32_t ino_t;
typedef uint8_t ftype_t;
typedef size_t off_t;

struct vnode {
	char name[256];
	uint32_t flags;
	uint32_t gid;
	uint32_t uid;
	uint32_t size;
	uint32_t ino_num;

	struct fs *fs;
	struct vnode *ptr;
	struct vnode *next;

	struct vnode *parent;

	struct dirent *dirents; /* for directories */
	size_t num_dirents;

	bool mount_ptr; /* does this node point to a root of a mounted fs? */
	bool no_free; /* permanently cached vnodes */

	void *priv_data;

	size_t refcount;
	spinlock_t lock;
};

struct dirent {
	uint64_t inode; /* fs-specific inode number */
	uint16_t reclen; /* length of this record */
	ftype_t type; /* type: file, dir, chrdev, blkdev, etc. */
	char name[256];

	struct vnode *vnode; /* we don't want duplicate vnodes */
};

struct file {
	struct vnode *vnode;

	spinlock_t ref_lock;
	uint64_t refcount;

	ino_t ino_num; /* inode number */
	uint32_t type; /* type of the file */
	uint64_t size; /* size of the file */
};

struct file_descriptor {
	struct file *file;

	int fd;
	uint64_t pos;
	uint32_t flags;

	char *buf;
	size_t buf_len;

	struct fs *fs;
};

struct fs_ops {
	int (*read)(struct vnode *vnode, void *buf, size_t offset, size_t size);
	int (*write)(struct vnode *vnode, void *buf, size_t offset, size_t size);
	int (*unlink)(struct vnode *parent, const char *name);
	int (*creat)(struct vnode *parent, const char *path, mode_t mode);
	int (*open_vno)(struct fs *vfs, struct vnode *out, ino_t ino_num);
};

struct fs {
	struct vnode *root;
	void *fs;
	uint32_t type;
	struct fs_ops *ops;

	char *mount_point;
};

struct statbuf {
	size_t size;
};

void vfs_init(const char *rootfs);
ssize_t vfs_write(struct file *file, void *buf, off_t off, size_t count);
ssize_t vfs_read(struct file *file, void *buf, off_t off, size_t count);
int vfs_statf(struct file *file, struct statbuf *statbuf);
struct file *vfs_open(const char *pathname, int *err);
struct vnode *vfs_create_file(struct vnode *parent, const char *path, mode_t mode);
struct vnode *vfs_mknod(const char *pathname, mode_t mode);
int vfs_close(struct file *file);
int unlink(const char *pathname);
int statfd(struct file_descriptor *fdesc, struct statbuf *statbuf);

char *basename(char *path);
char *dirname(char *path);

struct fs *vfs_create();
struct vnode *vfs_create_vno();
void vfs_dealloc(struct fs *fs);
int vfs_mount(struct fs *fs, const char *mount_point);

uint64_t vfs_file_type(struct file *file);
void *vfs_file_dev(struct file *file);

int stdout_write(struct file_descriptor *fdesc, void *buf, size_t len);

#endif /* _VFS_H_ */
