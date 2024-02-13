/* SDPX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/rbtree.h>

#include <fs/ext2.h>
#include <fs/vfs.h>

#include <lib/stack.h>

static struct fs *rootfs;
static slab_t *file_slab;
static slab_t *vnode_slab;

typedef struct fs *(*vfs_init_t)(struct block_device *);

static void vfs_vnode_dec_ref(struct vnode *vnode);
static void vfs_vnode_dealloc(struct vnode *vnode);

static void vfs_vnode_dealloc(struct vnode *vnode)
{
	if (!vnode)
		return;

	if (vnode->no_free)
		return;

	if (vnode->parent) {
		spinlock_acquire(&vnode->parent->lock);
		for (size_t i = 0; i < vnode->parent->num_dirents; i++) {
			if (vnode->parent->dirents[i].vnode == vnode) {
				vnode->parent->dirents[i].vnode = NULL;
				break;
			}
		}
		spinlock_release(&vnode->parent->lock);

		vfs_vnode_dec_ref(vnode->parent);
	}

	ATTEMPT_FREE(vnode->dirents);

	slab_free(vnode_slab, vnode);
}

static void vfs_vnode_dec_ref(struct vnode *vnode)
{
	if (!vnode)
		return;

	if (vnode->no_free)
		return;

	spinlock_acquire(&vnode->lock);
	vnode->refcount--;
	spinlock_release(&vnode->lock);

	/* will recursively free all vnodes */
	if (vnode->refcount == 0)
		vfs_vnode_dealloc(vnode);
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

	int len = strlen(pathname);
	int inc = 0;
	for (int i = 0; i < len; i++) {
		if (pathname[i] == '/')
			inc++;
		else
			break;
	}

	if (inc < len)
		pathname += inc;

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

					if (cur_vnode->mount_ptr) {
						fs = cur_vnode->ptr->fs;
						cur_vnode = cur_vnode->ptr;
					}

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
			vfs_vnode_dec_ref(cur_vnode);
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

	vfs_vnode_dec_ref(file->vnode);

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

struct vnode *vfs_create_vno()
{
	struct vnode *vnode = slab_alloc(vnode_slab);
	if (!vnode)
		return NULL;

	memset(vnode, 0, sizeof(struct vnode));

	return vnode;
}

struct fs *vfs_create()
{
	struct fs *fs = kzalloc(sizeof(struct fs), ALLOC_KERN);
	if (!fs)
		return NULL;

	fs->root = vfs_create_vno();
	if (!fs->root)
		goto out_root;

	fs->ops = kzalloc(sizeof(struct fs_ops), ALLOC_KERN);
	if (!fs->ops)
		goto out_ops;

	fs->root->no_free = true;
	fs->root->refcount = 1;

	return fs;

out_ops:
	kfree(fs->root);
out_root:
	kfree(fs);
	return NULL;
}

void vfs_dealloc(struct fs *fs)
{
	ATTEMPT_FREE(fs->ops);
	if (fs->root)
		ATTEMPT_FREE(fs->root->dirents);
	slab_free(vnode_slab, fs->root);

	ATTEMPT_FREE(fs->mount_point);
	ATTEMPT_FREE(fs);
}

void vfs_no_free_r(struct vnode *vnode)
{
	if (!vnode)
		return;

	while (vnode && !vnode->no_free) {
		vnode->no_free = true;
		vnode = vnode->parent;
	}
}

int vfs_mount(struct fs *fs, const char *mount_point)
{
	struct vnode *vnode = fs->root;

	/* find vnode of mount_point */
	int err;
	struct file *file = vfs_open(mount_point, &err);
	if (!file)
		return err;

	struct vnode *mp_vnode = file->vnode;

	/* mount point is a directory */
	if ((mp_vnode->flags & VFS_VTYPE_MASK) != VFS_VNO_DIR) {
		vfs_close(file);
		return -ENOTDIR;
	}

	/* mount point is empty */
	for (int i = 0; i < mp_vnode->num_dirents; i++) {
		struct dirent *dirent = &mp_vnode->dirents[i];
		if (strcmp(".", dirent->name) == 0 || strcmp("..", dirent->name) == 0)
			continue;
		kprintf("dirent: %s\n", dirent->name);
	}

	/* mount */
	spinlock_acquire(&mp_vnode->lock);
	spinlock_acquire(&vnode->lock);

	mp_vnode->ptr = vnode;
	mp_vnode->mount_ptr = true;

	vnode->parent = mp_vnode;
	vfs_no_free_r(vnode);

	spinlock_release(&vnode->lock);
	spinlock_release(&mp_vnode->lock);

	return 0;
}

struct fs *vfs_mount_root(const char *dev, const char *mount_point)
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
	if (!fs->mount_point)
		goto out_dealloc_fs;

	strcpy(fs->mount_point, mount_point);

	struct vnode *root = fs->root;
	struct dirent *dirents = NULL;
	int num_dirents = fs->ops->readdir(root, &dirents);
	if (num_dirents < 0)
		goto out_dealloc_fs;

	root->dirents = dirents;
	root->num_dirents = num_dirents;
	root->no_free = true;
	return fs;

out_dealloc_fs:
	vfs_dealloc(fs);
out:
	return NULL;
}

void vfs_init(const char *rootdev_name)
{
	file_slab = slab_create(sizeof(struct file), 16 * KB, 0);
	vnode_slab = slab_create(sizeof(struct vnode), 16 * KB, 0);

#ifdef KDEBUG
	kprintf(LOG_DEBUG "Found root device %s\n", rootdev_name);
#endif

	struct fs *fs = vfs_mount_root(rootdev_name, "/");
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
}
