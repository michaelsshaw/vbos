/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>

#include <fs/vfs.h>

static struct rbtree *kfd;
static slab_t *fd_slab;
static struct fs *rootfs;
static int fd_counter = 0;

typedef struct fs *(*vfs_init_t)(struct block_device *);

int write(int fd, const void *buf, size_t count)
{
	return 0;
}

int read(int fd, void *buf, size_t count)
{
	return 0;
}

int open(const char *pathname, int flags)
{
	/* allocate a file descriptor */
	struct file_descriptor *fd = slab_alloc(fd_slab);
	if (!fd) {
		kprintf(LOG_ERROR "Failed to allocate file descriptor\n");
		return -ENOMEM;
	}

	/* find the file */
	int result = rootfs->ops.open(rootfs, &fd->file, pathname);

	if (result < 0) {
		kprintf(LOG_ERROR "Failed to open %s: %s\n", pathname, strerror(-result));
		return result;
	} else if (result == VFSE_IS_BDEV) {
		/* TODO: continue search into mounted filesystems */
	}

	/* add the file descriptor to the tree */
	fd->fd = fd_counter;
	fd->fs = rootfs;
	fd_counter += 1;

	struct rbnode *fdnode = rbt_insert(kfd, fd->fd);
	fdnode->value = (uint64_t)fd;

	return fd->fd;
}

int close(int fd)
{
	struct rbnode *fdnode = rbt_search(kfd, fd);

	if (!fdnode)
		return -EBADF;

	struct file_descriptor *fdesc = (struct file_descriptor *)fdnode->value;
	fdesc->fs->ops.close(fdesc->fs, &fdesc->file);
	kfree(fdesc->file.path);

	slab_free(fd_slab, fdesc);
	rbt_delete(kfd, fdnode);

	return 0;
}

int seek(int fd, size_t offset, int whence)
{
	return 0;
}

DIR *opendir(const char *name)
{
	int fd = open(name, 0);
	if (fd < 0)
		return NULL;

	struct file_descriptor *fdesc = (struct file_descriptor *)rbt_search(kfd, fd)->value;

	if (!fdesc)
		return NULL;

	struct fs *fs = fdesc->fs;
	struct file *file = &fdesc->file;

	if (file->type != VFS_FILE_DIR) {
		close(fd);
		return NULL;
	}

	DIR *dir = kzalloc(sizeof(DIR), ALLOC_KERN);
	dir->fd = fd;
	dir->fs = fs;
	dir->file = file;
	dir->pos = 0;
	dir->dirents = NULL;

	return dir;
}

struct dirent *readdir(DIR *dir)
{
	if(!dir)
		return NULL;

	struct fs *fs = dir->fs;
	struct file *file = dir->file;

	if (file->type != VFS_FILE_DIR)
		return NULL;

	if (dir->dirents == NULL)
		dir->num_dirents = fs->ops.readdir(fs, file->inode_num, &dir->dirents);

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

	int ret = close(dir->fd);
	kfree(dir->dirents);
	kfree(dir);

	return ret;
}

void vfs_init(const char *rootdev_name)
{
	kfd = kzalloc(sizeof(struct rbtree), ALLOC_DMA);

	/* mount root */
	struct block_device *rootdev = block_get_device(rootdev_name);
	if (!rootdev) {
		kprintf(LOG_ERROR "Failed to find root device %s\n", rootdev_name);
		return;
	}

	kprintf(LOG_DEBUG "Found root device %s\n", rootdev_name);

	/* TODO: detect filesystem type */
	/* for now, assume ext2 */
	struct fs *fs = ext2_init_fs(rootdev);
	if (!fs) {
		kprintf(LOG_ERROR "Failed to mount root device %s\n", rootdev_name);
		return;
	} else {
		kprintf(LOG_DEBUG "Mounted root device %s\n", rootdev_name);
	}

	fs->mount_point = kzalloc(2, ALLOC_KERN);
	fs->mount_point[0] = '/';

	rootfs = fs;

	/* init file descriptor slab */
	fd_slab = slab_create(sizeof(struct file_descriptor), 16 * KB, 0);
}

