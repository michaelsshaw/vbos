/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>

#define SYS_EXIT 60

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
	syscall_insert(SYS_EXIT, (syscall_t)sys_exit);
}
