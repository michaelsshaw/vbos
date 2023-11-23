/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>
#include <kernel/syscall.h>

#include <fs/vfs.h>

static struct rbtree *syscall_tree;

typedef void (*syscall_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

void syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	struct rbnode *node = rbt_search(syscall_tree, syscall_no);
	if (node == NULL) {
		return;
	}

	syscall_t syscall = (void *)node->value;

	syscall(arg1, arg2, arg3, arg4, arg5);
}

int sys_read(int fd, void *buf, size_t count)
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

	return read(fdesc, buf, count);
}

int sys_write(int fd, void *buf, size_t count)
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

	return write(fdesc, buf, count);
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
	struct rbnode *node = rbt_insert(syscall_tree, syscall_no);
	node->value = (uintptr_t)syscall;
}

void syscall_init()
{
	syscall_tree = kzalloc(sizeof(struct rbtree), ALLOC_KERN);
	syscall_insert(SYS_WRITE, (syscall_t)sys_write);
	syscall_insert(SYS_EXIT, (syscall_t)sys_exit);
}
