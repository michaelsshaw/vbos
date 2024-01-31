/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>
#include <fs/vfs.h>

#include <lib/stack.h>

static struct rbtree *kfd;
static struct fs *rootfs;
static slab_t *file_slab;
static slab_t *vnode_slab;

typedef struct fs *(*vfs_init_t)(struct block_device *);

static void vfs_vnode_dealloc(struct vnode *vnode)
{
	if (!vnode)
		return;
	if (vnode->no_free)
		return;

	ATTEMPT_FREE(vnode->dirents);

	slab_free(vnode_slab, vnode);
}

static void vfs_dealloc(struct fs *fs)
{
	ATTEMPT_FREE(fs->ops);
	if (fs->root)
		ATTEMPT_FREE(fs->root->dirents);
	ATTEMPT_FREE(fs->root);

	ATTEMPT_FREE(fs->mount_point);
	ATTEMPT_FREE(fs);
}

struct file *vfs_open(const char *pathname, int *err)
{
	/* allocate a file */
	struct file *file = slab_alloc(file_slab);
	if (!file) {
		kprintf(LOG_ERROR "Failed to allocate file struct\n");
		return NULL;
	}
	memset(file, 0, sizeof(struct file));

	/* terrible temporary hack */
	if (pathname == NULL && (uintptr_t)err == 0x4)
		return file;

	/* start at root */
	struct fs *fs = rootfs;
	struct vnode *cur_vnode = fs->root;

	char *path_copy = kmalloc(strlen(pathname) + 1, ALLOC_KERN);
	strcpy(path_copy, pathname);

	char *tok_last = NULL;
	char *tok = strtok(path_copy, "/", &tok_last);

	struct dirent *entry = NULL;

	while (!strempty(tok)) {
		bool found = false;
		for (int i = 0; i < cur_vnode->num_dirents; i++) {
			entry = &cur_vnode->dirents[i];
			if (strcmp(entry->name, tok) == 0) {
				if (entry->vnode) {
					/* go to next vnode */
					cur_vnode = entry->vnode;
					spinlock_acquire(&cur_vnode->lock);
					cur_vnode->refcount++;
					spinlock_release(&cur_vnode->lock);
				} else {
					/* try to open the vnode */
					struct vnode *new_vnode = slab_alloc(vnode_slab);
					if (!new_vnode) {
						*err = -ENOMEM;
						goto out_2;
					}
					memset(new_vnode, 0, sizeof(struct vnode));
					new_vnode->refcount = 1;
					new_vnode->parent = cur_vnode;

					int res = fs->ops->open_vno(fs, new_vnode, entry->inode);
					if (res < 0) {
						*err = res;
						slab_free(vnode_slab, new_vnode);
						goto out_2;
					}

					if (new_vnode->flags & VFS_VNO_DIR) {
						int res = fs->ops->open_dir(new_vnode->fs, new_vnode);
						if (res < 0) {
							*err = res;
							slab_free(vnode_slab, new_vnode);
							goto out_2;
						}
					}

					cur_vnode = new_vnode;
					entry->vnode = cur_vnode;
				}

				found = true;
				break;
			}
		}

		if (!found) {
			*err = -ENOENT;
			/* clean up */
			while (cur_vnode) {
				spinlock_acquire(&cur_vnode->lock);
				cur_vnode->refcount--;
				spinlock_release(&cur_vnode->lock);

				if (cur_vnode->refcount == 0)
					vfs_vnode_dealloc(cur_vnode);

				cur_vnode = cur_vnode->parent;
			}
			goto out_2;
		}

		/* get next token */
		tok = strtok(NULL, "/", &tok_last);
	}

	file->vnode = cur_vnode;
	file->type = cur_vnode->flags & VFS_VTYPE_MASK;
	file->size = cur_vnode->size;
	file->ino_num = cur_vnode->ino_num;

	return file;

out_2:
	slab_free(file_slab, file);
	return NULL;
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

	/* read dirents from fs root */

	/* root vnode is allocated by the FS driver with kmalloc
	 * all other vnodes are allocated by the VFS with slab_alloc 
	 */
	struct vnode *root = fs->root;
	struct dirent *dirents = NULL;
	int num_dirents = fs->ops->readdir(root, &dirents);
	if (num_dirents < 0) {
		kprintf(LOG_ERROR "Failed to read root directory\n");
		vfs_dealloc(fs);
		fs = NULL;
		goto out;
	}

	root->dirents = dirents;
	root->num_dirents = num_dirents;
	root->no_free = true;

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
