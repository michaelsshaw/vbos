/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>

#include <dev/apic.h>

#include <fs/vfs.h>

static slab_t *proc_slab;
static struct rbtree *proc_tree;
static pid_t *proc_current;

static struct proc kernel_procs[256] = { 0 };

void _return_to_user(struct procregs *regs, paddr_t cr3);
extern uintptr_t kstacks[256];

struct procregs *proc_current_regs()
{
	if (proc_current[lapic_idno()] == 0)
		return &kernel_procs[lapic_idno()].regs;

	struct rbnode *proc_node = rbt_search(proc_tree, proc_current[lapic_idno()]);

	if (proc_node) {
		struct proc *ret = (void *)proc_node->value;

		return &ret->regs;
	} else {
		return NULL;
	}
}

pid_t getpid()
{
	return proc_current[lapic_idno()];
}

struct proc *proc_find(pid_t pid)
{
	if (pid == 0)
		return &kernel_procs[lapic_idno()];

	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	if (proc_node)
		return (void *)proc_node->value;
	else
		return NULL;
}

void proc_set_current(pid_t pid)
{
	proc_current[lapic_idno()] = pid;
}

void proc_term(pid_t pid)
{
	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	if (proc_node) {
		struct proc *proc = (void *)proc_node->value;

		rbt_delete(proc_tree, proc_node);

		struct rbnode *map_node = rbt_minimum(proc->page_map.root);
		while (map_node) {
			struct rbnode *next = rbt_successor(map_node);
			proc_munmap(proc, map_node->key);
			map_node = next;
		}

		slab_free(proc_slab, proc);
	} else {
		kprintf(LOG_ERROR "proc: proc_term: proc %u not found\n", pid);
		panic();
	}
}

void schedule()
{
	uint8_t id = lapic_idno();

	struct proc *proc = proc_find(proc_current[id]);

	if (proc) {
		_return_to_user(&proc->regs, proc->cr3);
	} else {
		kprintf(LOG_ERROR "proc: schedule: proc_current[%u] is NULL\n", id);
		panic();
	}
}

static int proc_insert_charfd(struct proc *proc, int fdno, int flags)
{
	struct file_descriptor *fd = fd_special();
	fd->vnode = (struct vnode){ 0 };
	fd->pos = 0;
	fd->fd = fdno;
	fd->flags = flags;
	fd->mode = FD_TYPE_CHARDEV;
	fd->buf = NULL;
	fd->buf_size = 0;
	fd->fs = NULL;

	struct rbnode *fd_node = rbt_insert(&proc->fd_map, fdno);
	fd_node->value = (uintptr_t)fd;

	return fdno;
}

struct proc *proc_create()
{
	struct proc *proc = slab_alloc(proc_slab);
	memset(proc, 0, sizeof(struct proc));

	proc->pid = rbt_next_key(proc_tree);
	proc->is_kernel = false;

	struct rbnode *proc_node = rbt_insert(proc_tree, proc->pid);
	proc_node->value = (uintptr_t)proc;

	/* insert stdin, stdout, stderr */
	proc_insert_charfd(proc, 0, O_RDONLY); /* stdin */
	proc_insert_charfd(proc, 1, O_WRONLY); /* stdout */
	proc_insert_charfd(proc, 2, O_WRONLY); /* stderr */

	return proc;
}

struct proc *proc_get(pid_t pid)
{
	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	if (proc_node)
		return (void *)proc_node->value;
	else
		return NULL;
}

void proc_init(unsigned num_cpus)
{
	/* allocate current process structs */
	proc_current = kzalloc(num_cpus * sizeof(pid_t), ALLOC_KERN);
	proc_tree = kzalloc(sizeof(struct rbtree), ALLOC_KERN);
	proc_slab = slab_create(sizeof(struct proc), 32 * KB, 0);

	for (unsigned i = 0; i < num_cpus; i++) {
		proc_current[i] = -1;

		struct proc *proc = &kernel_procs[i];

		proc->pid = 0;
		proc->is_kernel = true;
		proc->cr3 = kcr3;

		proc->regs.rip = (uint64_t)yield;
		proc->regs.cs = 0x28;
		proc->regs.rflags = 0x246;
		proc->regs.rsp = kstacks[i] + 0xFF0;

		proc_current[i] = 0;
	}

	/* ensure that the first process has PID 0
	 * this value is never actually used, it prevents other processes from
	 * having PID 0
	 */
	(void)rbt_insert(proc_tree, 0);
}
