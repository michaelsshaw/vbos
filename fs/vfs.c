/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>

#include <fs/vfs.h>

static struct rbtree *kfd;
static struct fs *rootfs;
static slab_t *file_slab;
static slab_t *vnode_slab;

typedef struct fs *(*vfs_init_t)(struct block_device *);

struct file *vfs_open(const char *pathname, int *err)
{
	/* allocate a file */
	struct file *file = slab_alloc(file_slab);
	if (!file) {
		kprintf(LOG_ERROR "Failed to allocate file struct\n");
		return NULL;
	}

	struct vnode *vnode = slab_alloc(vnode_slab);
	if (!vnode) {
		kprintf(LOG_ERROR "Failed to allocate vnode struct\n");
		slab_free(file_slab, file);
		return NULL;
	}

	memset(file, 0, sizeof(struct file));

	file->vnode = vnode;

	/* terrible temporary hack */
	if (pathname == NULL && (uintptr_t)err == 0x4)
		return file;

	/* find the file */
	int result = rootfs->ops->open(rootfs, vnode, pathname);

	if (result < 0) {
		*err = result;
		slab_free(file_slab, file);
		slab_free(vnode_slab, vnode);
		return NULL;
	}

	return file;
}

int vfs_close(struct file *file)
{
	if (!file)
		return -EBADF;

	if (!file->vnode->no_free)
		slab_free(vnode_slab, file->vnode);

	slab_free(file_slab, file);
	return 0;
}

ssize_t vfs_write(struct file *file, void *buf, off_t off, size_t count)
{
	/* TODO: implement */
	if (file->type == FTYPE_CHARDEV)
		return -EBADF;

	if ((file->vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EISDIR;

	return file->vnode->fs->ops->write(file->vnode, buf, off, count);
}

ssize_t vfs_read(struct file *file, void *buf, off_t off, size_t count)
{
	/* TODO: implement */
	if (file->type == FTYPE_CHARDEV)
		return -EBADF;

	/* TODO: implement */
	if ((file->vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_REG)
		return -EISDIR;

	if (off >= file->vnode->size)
		return 0;

	if (off + count > file->vnode->size)
		count = file->vnode->size - off;

	return file->vnode->fs->ops->read(file->vnode, buf, off, count);
}

int vfs_statf(struct file *file, struct statbuf *statbuf)
{
	statbuf->size = file->vnode->size;

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

	struct fs *fs = file->vnode->fs;

	if ((file->vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_DIR) {
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

struct dirent *readdir(DIR *dir)
{
	if (!dir)
		return NULL;

	struct fs *fs = dir->fs;

	if ((dir->file->vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_DIR)
		return NULL;

	if (dir->dirents == NULL)
		dir->num_dirents = fs->ops->readdir(dir->file->vnode, &dir->dirents);

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

struct fs *vfs_mount(const char *dev, const char *mount_point)
{
	/* TODO: detect filesystem type */
	/* for now, assume ext2 */
	struct block_device *bdev = block_get_device(dev);
	if (!dev) {
		kprintf(LOG_ERROR "Failed to find device %s\n", dev);
		kerror_print_blkdevs();
		panic();
	}
	struct fs *fs = ext2_init_fs(bdev);

	if (!fs)
		goto out;

	fs->type = FS_TYPE_EXT2;

	fs->mount_point = kmalloc(strlen(mount_point) + 1, ALLOC_KERN);
	strcpy(fs->mount_point, mount_point);

out:
	return fs;
}

void vfs_init(const char *rootdev_name)
{
	kfd = kzalloc(sizeof(struct rbtree), ALLOC_DMA);

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Found root device %s\n", rootdev_name);
#endif

	struct fs *fs = vfs_mount(rootdev_name, "/");
	if (!fs) {
		kprintf(LOG_ERROR "Failed to mount root device %s\n", rootdev_name);
		kerror_print_blkdevs();
		panic();
	} else {
#ifdef KDEBUG
		kprintf(LOG_DEBUG "Mounted root device %s\n", rootdev_name);
#endif
	}

	rootfs = fs;

	/* init file descriptor slab */
	file_slab = slab_create(sizeof(struct file), 16 * KB, 0);
	vnode_slab = slab_create(sizeof(struct vnode), 16 * KB, 0);
}
