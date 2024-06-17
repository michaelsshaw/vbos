/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>
#include <kernel/gdt.h>

#include <lib/sem.h>

#include <dev/apic.h>

#include <fs/vfs.h>

static slab_t *proc_slab;
static struct rbtree *proc_tree;
static pid_t *proc_current;

static struct proc kernel_procs[256] = { 0 };

static struct proc_block_queue *proc_block_queue;

void _return_to_user(struct procregs *regs, paddr_t cr3);
extern uintptr_t kstacks[256];
extern slab_t *fd_slab;

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

struct proc *proc_next(struct proc *proc)
{
	struct rbnode *proc_node = rbt_search(proc_tree, proc->pid);

	if (proc_node)
		proc_node = rbt_successor(proc_node);

	if (proc_node) {
		if (proc_node->key == 0)
			goto kernel_proc;

		return (void *)proc_node->value;
	} else {
kernel_proc:
		return &kernel_procs[lapic_idno()];
	}
}

void proc_set_current(pid_t pid)
{
	proc_current[lapic_idno()] = pid;
}

void proc_term(pid_t pid)
{
	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	struct proc_block_node *cur = proc_block_queue->head;
	while (cur) {
		if (cur->proc->pid == pid) {
			proc_block_remove(cur);
			cur = cur->next;
			kfree(cur);
			continue;
		}

		cur = cur->next;
	}

	if (proc_node) {
		struct proc *proc = (void *)proc_node->value;
		spinlock_acquire(&proc->lock);

		rbt_delete(proc_tree, proc_node);

		struct rbnode *node = rbt_minimum(proc->page_map.root);
		while (node) {
			struct rbnode *next = rbt_successor(node);
			proc_munmap(proc, node->key);
			node = next;
		}

		node = rbt_minimum(proc->fd_map.root);
		while (node) {
			struct rbnode *next = rbt_successor(node);
			struct file_descriptor *fdesc = (void *)node->value;
			vfs_close(fdesc->file);
			slab_free(fd_slab, fdesc);
			node = next;
		}

		rbt_destroy(&proc->page_map);
		rbt_destroy(&proc->fd_map);

		slab_free(proc_slab, proc);
	} else {
		kprintf(LOG_ERROR "proc: proc_term: proc %u not found\n", pid);
		panic();
	}
}

void trap_sched()
{
	lapic_eoi();
	schedule();
}

void schedule()
{
	uint8_t id = lapic_idno();

	struct proc *proc = proc_find(proc_current[id]);

	if (proc && proc->state == PROC_RUNNING) {
		spinlock_acquire(&proc->lock);
		proc->state = PROC_STOPPED;
		spinlock_release(&proc->lock);
	}

	size_t counter = 0;
	size_t limit = 1000000;
	while (counter++ < limit) {
		/* next process */
		proc = proc_next(proc);

		spinlock_acquire(&proc->lock);

		switch (proc->state) {
		/* fallthrough */
		case PROC_STOPPED:
			proc->state = PROC_RUNNING;
			spinlock_release(&proc->lock);
			_return_to_user(&proc->regs, proc->cr3);

			break;
		case PROC_BLOCKED:
		case PROC_RUNNING:
			spinlock_release(&proc->lock);
			break;
		}
	}

	sti();
	yield();
}

void proc_block(struct proc *proc, void *dev, void *buf, bool read, size_t size)
{
	struct proc_block_node *insert = kmalloc(sizeof(struct proc_block_node), ALLOC_KERN);
	insert->proc = proc;
	insert->dev = dev;
	insert->buf = buf;
	insert->read = read;
	insert->size = size;

	/* insert into the queue */
	struct proc_block_node *cur = proc_block_queue->head;

	spinlock_acquire(&proc_block_queue->lock);
	if (cur == NULL) {
		proc_block_queue->head = insert;
	} else {
		while (cur->next)
			cur = cur->next;

		cur->next = insert;
	}
	spinlock_release(&proc_block_queue->lock);

	proc->state = PROC_BLOCKED;
}

struct proc_block_node *proc_block_find(void *dev)
{
	void *ret = NULL;
	struct proc_block_node *cur = proc_block_queue->head;

	spinlock_acquire(&proc_block_queue->lock);
	while (cur) {
		if (cur->dev == dev) {
			ret = cur;
			break;
		}

		cur = cur->next;
	}
	spinlock_release(&proc_block_queue->lock);

	return ret;
}

void proc_block_remove(struct proc_block_node *node)
{
	struct proc_block_node *cur = proc_block_queue->head;

	spinlock_acquire(&proc_block_queue->lock);
	if (cur == node) {
		proc_block_queue->head = node->next;
		kfree(node);
	} else {
		while (cur) {
			if (cur->next == node) {
				cur->next = node->next;
				kfree(node);
				break;
			}

			cur = cur->next;
		}
	}
	spinlock_release(&proc_block_queue->lock);
}

void proc_unblock(pid_t pid)
{
	struct proc *proc = proc_find(pid);

	if (proc) {
		spinlock_acquire(&proc->lock);
		proc->state = PROC_STOPPED;
		spinlock_release(&proc->lock);
	}
}

struct proc *proc_create()
{
	struct proc *proc = slab_alloc(proc_slab);
	memset(proc, 0, sizeof(struct proc));

	spinlock_acquire(&proc->lock);

	proc->pid = rbt_next_key(proc_tree);
	proc->is_kernel = false;

	struct rbnode *proc_node = rbt_insert(proc_tree, proc->pid);
	proc_node->value = (uintptr_t)proc;

	spinlock_release(&proc->lock);

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

	proc_block_queue = kzalloc(sizeof(struct proc_block_queue), ALLOC_KERN);

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
