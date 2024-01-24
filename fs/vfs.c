/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>

#include <fs/vfs.h>

static struct rbtree *kfd;
static slab_t *fd_slab;
static slab_t *file_slab;
static struct fs *rootfs;
static int fd_counter = 0;

typedef struct fs *(*vfs_init_t)(struct block_device *);

int write(struct file_descriptor *fdesc, void *buf, size_t count)
{
	if (fdesc->mode == FD_TYPE_CHARDEV) {
		if (fdesc->buf_write)
			return fdesc->buf_write(fdesc, buf, count);
		return -EBADF;
	}

	if ((fdesc->vnode.flags & VFS_VTYPE_MASK) == VFS_VNO_DIR)
		return -EISDIR;

	if ((fdesc->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EBADF;

	if (fdesc->flags & O_RDONLY)
		return -EBADF;

	int ret = fdesc->fs->ops->write(fdesc->fs, &fdesc->vnode, buf, fdesc->pos, count);

	if (ret < 0)
		return ret;

	fdesc->pos = MIN(fdesc->pos + ret, fdesc->vnode.size);

	return ret;
}

int read(struct file_descriptor *fdesc, void *buf, size_t count)
{
	if (fdesc->mode == FD_TYPE_CHARDEV) {
		if (fdesc->buf_read)
			return fdesc->buf_read(fdesc, buf, count);
		return -EBADF;
	}

	if ((fdesc->vnode.flags & VFS_VTYPE_MASK) == VFS_VNO_DIR)
		return -EISDIR;

	if ((fdesc->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EBADF;

	if (fdesc->flags & O_WRONLY)
		return -EBADF;

	int ret = fdesc->fs->ops->read(fdesc->fs, &fdesc->vnode, buf, fdesc->pos, count);

	if (ret < 0)
		return ret;

	fdesc->pos = MIN(fdesc->pos + ret, fdesc->vnode.size);

	return ret;
}

ssize_t vfs_write(struct file *file, void *buf, off_t off, size_t count)
{
	/* TODO: implement */
	if ((file->vnode.flags & VFS_VTYPE_MASK) == VFS_VNO_CHARDEV)
		return -EBADF;

	if ((file->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EISDIR;

	return file->vnode.fs->ops->write(file->vnode.fs, &file->vnode, buf, off, count);
}

ssize_t vfs_read(struct file *file, void *buf, off_t off, size_t count)
{
	/* TODO: implement */
	if ((file->vnode.flags & VFS_VTYPE_MASK) == VFS_VNO_CHARDEV)
		return -EBADF;

	/* TODO: implement */
	if ((file->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EISDIR;

	return file->vnode.fs->ops->read(file->vnode.fs, &file->vnode, buf, off, count);
}

int vfs_statf(struct file *file, struct statbuf *statbuf)
{
	statbuf->size = file->vnode.size;

	return 0;
}

int statfd(struct file_descriptor *fdesc, struct statbuf *statbuf)
{
	statbuf->size = fdesc->vnode.size;

	return 0;
}

struct file *vfs_open(const char *pathname, int *err)
{
	/* allocate a file */
	struct file *file = slab_alloc(file_slab);
	if (!file) {
		kprintf(LOG_ERROR "Failed to allocate file descriptor\n");
		return NULL;
	}

	memset(file, 0, sizeof(struct file));

	/* find the file */
	int result = rootfs->ops->open(rootfs, &file->vnode, pathname);

	if (result < 0) {
		*err = result;
		slab_free(file_slab, file);
		return NULL;
	} else if (result == VFSE_IS_BDEV) {
		/* TODO: continue search into mounted filesystems */
	} else {
	}

	return file;
}

struct file_descriptor *open(const char *pathname, int flags)
{
	/* allocate a file descriptor */
	struct file_descriptor *fd = slab_alloc(fd_slab);
	if (!fd) {
		kprintf(LOG_ERROR "Failed to allocate file descriptor\n");
		return NULL;
	}

	memset(fd, 0, sizeof(struct file_descriptor));

	/* find the file */
	int result = rootfs->ops->open(rootfs, &fd->vnode, pathname);

	if (result < 0) {
		fd->fd = result;
		return fd;
	} else if (result == VFSE_IS_BDEV) {
		/* TODO: continue search into mounted filesystems */
	}

	/* add the file descriptor to the tree */
	fd->fd = fd_counter;
	fd->fs = rootfs;
	fd_counter += 1;

	struct rbnode *fdnode = rbt_insert(kfd, fd->fd);
	fdnode->value = (uint64_t)fd;

	return fd;
}

int close(struct file_descriptor *fdesc)
{
	struct rbnode *node = rbt_search(kfd, fdesc->fd);
	rbt_delete(kfd, node);
	slab_free(fd_slab, fdesc);

	return 0;
}

int vfs_close(struct file *file)
{
	slab_free(file_slab, file);
	return 0;
}

int unlink(const char *pathname)
{
	return rootfs->ops->unlink(rootfs, pathname);
}

int mkdir(const char *pathname)
{
	return rootfs->ops->mkdir(rootfs, pathname);
}

DIR *opendir(const char *name)
{
	int err;
	struct file *file = vfs_open(name, &err);
	if (!file) {
		kprintf(LOG_ERROR "Failed to open directory %s: %s\n", name, strerror(err));
		return NULL;
	}

	struct fs *fs = file->vnode.fs;

	if ((file->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_DIR) {
		vfs_close(file);
		return NULL;
	}

	DIR *dir = kzalloc(sizeof(DIR), ALLOC_KERN);
	dir->file = file;
	dir->fs = fs;
	dir->pos = 0;
	dir->dirents = NULL;

	return dir;
}

struct file_descriptor *fd_special()
{
	struct file_descriptor *fd = slab_alloc(fd_slab);
	if (!fd) {
		kprintf(LOG_ERROR "Failed to allocate file descriptor\n");
		return NULL;
	}

	memset(fd, 0, sizeof(struct file_descriptor));

	return fd;
}

void fd_special_free(struct file_descriptor *fd)
{
	slab_free(fd_slab, fd);
}

struct dirent *readdir(DIR *dir)
{
	if (!dir)
		return NULL;

	struct fs *fs = dir->fs;

	if ((dir->file->vnode.flags & VFS_VTYPE_MASK) != VFS_VNO_DIR)
		return NULL;

	if (dir->dirents == NULL)
		dir->num_dirents = fs->ops->readdir(fs, dir->file->vnode.ino_num, &dir->dirents);

	if (dir->dirents == NULL)
		return NULL;

	if (dir->pos >= dir->num_dirents)
		return NULL;

	return &dir->dirents[dir->pos++];
}

int closedir(DIR *dir)
{
	if (!dir)
		return -EBADF;

	int ret = vfs_close(dir->file);
	kfree(dir->dirents);
	kfree(dir);

	return ret;
}

char *basename(char *path)
{
	char *ret = path;
	for (int i = 0; path[i]; i++) {
		if (path[i] == '/')
			ret = path + i + 1;
	}

	return ret;
}

char *dirname(char *path)
{
	char *ret = path;
	for (int i = 0; path[i]; i++) {
		if (path[i] == '/')
			ret = path + i;
	}

	if (ret == path)
		return NULL;

	*ret = 0;

	return path;
}

void vfs_init(const char *rootdev_name)
{
	kfd = kzalloc(sizeof(struct rbtree), ALLOC_DMA);

	/* mount root */
	struct block_device *rootdev = block_get_device(rootdev_name);
	if (!rootdev) {
		kprintf(LOG_ERROR "Failed to find root device %s\n", rootdev_name);
		kerror_print_blkdevs();
		panic();
	}

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Found root device %s\n", rootdev_name);
#endif

	/* TODO: detect filesystem type */
	/* for now, assume ext2 */
	struct fs *fs = ext2_init_fs(rootdev);
	if (!fs) {
		kprintf(LOG_ERROR "Failed to mount root device %s\n", rootdev_name);
		kerror_print_blkdevs();
		panic();
	} else {
#ifdef KDEBUG
		kprintf(LOG_DEBUG "Mounted root device %s\n", rootdev_name);
#endif
	}

	fs->mount_point = kzalloc(2, ALLOC_KERN);
	fs->mount_point[0] = '/';

	rootfs = fs;

	/* init file descriptor slab */
	fd_slab = slab_create(sizeof(struct file_descriptor), 16 * KB, 0);
	file_slab = slab_create(sizeof(struct file), 16 * KB, 0);
}
