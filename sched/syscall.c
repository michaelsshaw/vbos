/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>
#include <kernel/syscall.h>

#include <fs/vfs.h>

#define SYSCALL_MAX 0x100

typedef void (*syscall_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

syscall_t syscall_table[SYSCALL_MAX];
slab_t *fd_slab;

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


void syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	syscall_t syscall = (void *)syscall_table[syscall_no];

	syscall(arg1, arg2, arg3, arg4, arg5);
}

ssize_t sys_read(int fd, void *buf, size_t count)
{
	if ((uintptr_t)buf >= hhdm_start)
		return -EFAULT;

	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return -1;

	struct rbnode *fdesc_node = rbt_search(&proc->fd_map, fd);

	if (fdesc_node == NULL)
		return -EBADF;

	struct file_descriptor *fdesc = (void *)fdesc_node->value;
	struct file *file = fdesc->file;
	void *tmp_buf = kzalloc(count, ALLOC_KERN);
	if (tmp_buf == NULL)
		return -ENOMEM;

	ssize_t ret = vfs_read(file, tmp_buf, fdesc->pos, count);
	if (ret > 0)
		memcpy(buf, tmp_buf, count);
	else
		ret = -1;

	fdesc->pos += ret;

	kfree(tmp_buf);
	return ret;
}

ssize_t sys_write(int fd, void *buf, size_t count)
{
	if ((uintptr_t)buf >= hhdm_start)
		return -EFAULT;

	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return -1;

	struct rbnode *fdesc_node = rbt_search(&proc->fd_map, fd);

	if (fdesc_node == NULL)
		return -EBADF;

	struct file_descriptor *fdesc = (void *)fdesc_node->value;
	struct file *file = fdesc->file;

	void *tmp_buf = kzalloc(count, ALLOC_KERN);
	if (tmp_buf == NULL)
		return -ENOMEM;

	memcpy(tmp_buf, buf, count);
	ssize_t ret = vfs_write(file, tmp_buf, fdesc->pos, count);

	if (ret < 0)
		ret = -1;
	kfree(tmp_buf);
	return ret;
}

int sys_open(const char *pathname, int flags)
{
	if ((uintptr_t)pathname >= hhdm_start)
		return -EFAULT;

	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return -1;

	int err;
	struct file *file = vfs_open(pathname, &err);
	if (file == NULL)
		return err;

	struct file_descriptor *fdesc = slab_alloc(fd_slab);
	uint64_t fdno = rbt_next_key(&proc->fd_map);

	fdesc->fd = fdno;
	fdesc->file = file;
	fdesc->pos = 0;
	fdesc->flags = flags;

	struct rbnode *node = rbt_insert(&proc->fd_map, fdno);
	node->value = (uintptr_t)fdesc;

	return fdno;
}

void sys_exit(int status)
{
	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return;

	proc->state = PROC_STOPPED;

	proc_term(getpid());

	kprintf("(syscall)Process %d exited with status 0x%x\n", getpid(), status);

	proc_set_current(0);
	schedule();
}

static void syscall_insert(uint64_t syscall_no, syscall_t syscall)
{
	syscall_table[syscall_no] = syscall;
}

void syscall_init()
{
	fd_slab = slab_create(sizeof(struct file_descriptor), 16 * KB, 0);

	memset(syscall_table, 0, sizeof(syscall_table));
	syscall_insert(SYS_READ, (syscall_t)sys_read);
	syscall_insert(SYS_WRITE, (syscall_t)sys_write);
	syscall_insert(SYS_OPEN, (syscall_t)sys_open);
	syscall_insert(SYS_EXIT, (syscall_t)sys_exit);
}
