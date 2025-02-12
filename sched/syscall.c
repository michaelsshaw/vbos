/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/lock.h>
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>
#include <kernel/syscall.h>
#include <kernel/msr.h>
#include <kernel/gdt.h>

#include <fs/vfs.h>

#define SYSCALL_MAX 0x100

typedef uint64_t (*syscall_t)(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

syscall_t syscall_table[SYSCALL_MAX];
slab_t *fd_slab;

void gate_syscall();

uint64_t syscall(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t syscall_no)
{
	syscall_t sc_sel = (void *)syscall_table[syscall_no];

	if (sc_sel == NULL)
		return -ENOSYS;

	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return -1;

	uint64_t ret = sc_sel(arg1, arg2, arg3, arg4, arg5);

	return ret;
}

void sys_set_return(struct proc *proc, uint64_t ret)
{
	if (proc == NULL)
		return;

	proc->regs.rax = ret;
}

ssize_t sys_read(int fd, void *buf, size_t count)
{
	if ((uintptr_t)buf >= hhdm_start)
		return -EFAULT;

	struct proc *proc = proc_find(getupid());
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

	struct proc *proc = proc_find(getupid());
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

	struct proc *proc = proc_find(getupid());
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

	spinlock_acquire(&file->ref_lock);
	file->refcount++;
	spinlock_release(&file->ref_lock);

	struct rbnode *node = rbt_insert(&proc->fd_map, fdno);
	node->value = (uintptr_t)fdesc;

	return fdno;
}

void sys_exit(int status)
{
	pid_t pid = getupid();
	struct proc *proc = proc_find(pid);
	if (proc == NULL)
		return;

	proc_term(pid);

	proc_set_current(0);
	schedule();
}

pid_t sys_fork()
{
	struct proc *parent = proc_find(getupid());
	struct proc *proc = proc_create();

	proc_clone_mmap(parent, proc);

	/* copy file descriptors */
	struct rbnode *node = rbt_minimum(parent->fd_map.root);
	while (node) {
		struct file_descriptor *fdesc = (void *)node->value;
		struct file *file = fdesc->file;

		struct file_descriptor *new_fdesc = slab_alloc(fd_slab);
		new_fdesc->fd = fdesc->fd;
		new_fdesc->file = file;
		new_fdesc->pos = fdesc->pos;
		new_fdesc->flags = fdesc->flags;

		spinlock_acquire(&file->ref_lock);
		file->refcount++;
		spinlock_release(&file->ref_lock);

		struct rbnode *new_node = rbt_insert(&proc->fd_map, fdesc->fd);
		new_node->value = (uintptr_t)new_fdesc;

		node = rbt_successor(node);
	}

	/* copy parent */
	proc->parent = parent;
	proc->regs = parent->regs;
	sys_set_return(proc, 0);

	proc->state = PROC_STOPPED;

	return proc->pid;
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
	syscall_insert(SYS_FORK, (syscall_t)sys_fork);
	syscall_insert(SYS_EXIT, (syscall_t)sys_exit);

	/* init syscall instruction */
	uint64_t star = rdmsr(MSR_IA32_STAR);
	star |= ((uint64_t)GDT_SEGMENT_CODE_RING0 << 32);
	star |= ((uint64_t)(GDT_SEGMENT_CODE_RING3 - 16) << 48);
	wrmsr(MSR_IA32_STAR, star);

	wrmsr(MSR_IA32_LSTAR, (uint64_t)gate_syscall);

	uint64_t efer = rdmsr(MSR_IA32_EFER);
	wrmsr(MSR_IA32_EFER, efer | 0x1);

	wrmsr(MSR_IA32_FMASK, 0x200); /* disable interrupts */
}
