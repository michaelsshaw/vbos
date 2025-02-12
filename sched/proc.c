/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/rbtree.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/gdt.h>
#include <kernel/pio.h>

#include <lib/sem.h>

#include <dev/apic.h>

#include <fs/vfs.h>

static slab_t *proc_slab;
static struct rbtree *proc_tree;
static pid_t *proc_current;

static struct proc kernel_procs[256] = { 0 };

void _return_to_user(struct procregs *regs, paddr_t cr3);
extern uintptr_t kstacks[256];
extern slab_t *fd_slab;

static pid_t pid_counter = 0;
static pid_t kpid_counter = 0;

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

pid_t getupid()
{
	struct proc *proc = proc_find(getpid());

	if (proc && proc->buddy_proc)
		return proc->buddy_proc->pid;
	else
		return 0;
}

void proc_set_state(pid_t pid, uint8_t state)
{
	struct proc *proc = proc_find(pid);
	spinlock_acquire(&proc->lock);
	proc->state = state;
	spinlock_release(&proc->lock);
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

void proc_set_stack(struct proc *proc, uintptr_t base, size_t size)
{
	proc->stack_start = base;
	proc->stack_size = size;

	proc->regs.rsp = base + size - 0x10;
	proc->regs.rbp = base + size - 0x10;
}

void proc_set_flags(struct proc *proc, uint64_t flags)
{
	proc->regs.rflags = flags;
}

void proc_set_exec_addr(struct proc *proc, uintptr_t addr)
{
	proc->regs.rip = addr;
}

void proc_term(pid_t pid)
{
	if (pid == 1) {
		/* reboot */
		reboot();
	}

	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	if (proc_node) {
		struct proc *proc = (void *)proc_node->value;
		spinlock_acquire(&proc->lock);

		rbt_delete(proc_tree, proc_node);

		struct rbnode *node = rbt_minimum(proc->umalloc_tree.root);
		while (node) {
			struct rbnode *next = rbt_successor(node);
			ufree(proc, (void *)node->key);
			node = next;
		}

		node = rbt_minimum(proc->page_map.root);
		while (node) {
			struct rbnode *next = rbt_successor(node);
			proc_munmap(proc, node->key);
			node = next;
		}

		node = rbt_minimum(proc->fd_map.root);
		while (node) {
			struct rbnode *next = rbt_successor(node);
			struct file_descriptor *fdesc = (void *)node->value;

			spinlock_acquire(&fdesc->file->ref_lock);
			fdesc->file->refcount--;
			if (fdesc->file->refcount == 0)
				vfs_close(fdesc->file);
			spinlock_release(&fdesc->file->ref_lock);
			slab_free(fd_slab, fdesc);
			node = next;
		}

		buddy_free((void *)((paddr_t)proc->cr3 | hhdm_start));
		rbt_destroy(&proc->page_map);
		rbt_destroy(&proc->fd_map);

		slab_free(proc_slab, proc);
	} else {
		kprintf(LOG_ERROR "proc: proc_term: proc %d not found\n", pid);
		panic();
	}
}

void proc_yield()
{
	struct proc *proc = proc_find(getpid());
	proc_set_current(0);
	spinlock_acquire(&proc->lock);
	proc->state = PROC_STOPPED;
	spinlock_release(&proc->lock);
	sti();
	yield();
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

	size_t num_procs = proc_tree->num_nodes;
	size_t counter = 0;
	size_t limit = num_procs * 2;
	while (counter < limit) {
		/* next process */
		proc = proc_next(proc);

		if (proc->pid == 0)
			continue;

		spinlock_acquire(&proc->lock);

		switch (proc->state) {
		case PROC_STOPPED:

			proc->state = PROC_RUNNING;
			proc_set_current(proc->pid);
			spinlock_release(&proc->lock);
			_return_to_user(&proc->regs, proc->cr3);
			break;
		case PROC_ZOMBIE:
		case PROC_BLOCKED:
		case PROC_RUNNING:
			spinlock_release(&proc->lock);
			break;
		}
	}

	/* no process to run */
	sti();
	yield();
}

void proc_init_memory(struct proc *proc, uint64_t mem_flags)
{
	if (proc->is_kernel && proc->buddy_proc) {
		proc->cr3 = proc->buddy_proc->cr3;
	} else {
		proc->cr3 = (uintptr_t)buddy_alloc(0x1000);
		memcpy((void *)proc->cr3, (void *)(kcr3 | hhdm_start), 0x1000);
		proc->cr3 &= ~(hhdm_start);
	}

	uintptr_t stackaddr = (uintptr_t)buddy_alloc(0x4000);
	proc->stack_start = stackaddr;

	if (!proc->is_kernel) {
		stackaddr &= ~(hhdm_start);
		proc_mmap(proc, stackaddr | hhdm_start, stackaddr, 0x4000, PAGE_XD | PAGE_RW | PAGE_PRESENT);

		proc->regs.cs = GDT_SEGMENT_CODE_RING3 | 3;
		proc->regs.ss = GDT_SEGMENT_DATA_RING3 | 3;
	} else {
		proc->regs.cs = GDT_SEGMENT_CODE_RING0 | 0;
		proc->regs.ss = GDT_SEGMENT_DATA_RING0 | 0;
	}

	proc->regs.rflags = 0x246;
	proc_set_stack(proc, stackaddr, 0x4000);
}

struct proc *proc_createv(int flags)
{
	struct proc *proc = slab_alloc(proc_slab);
	memset(proc, 0, sizeof(struct proc));

	spinlock_acquire(&proc->lock);

	if (flags & PT_KERN) {
		kpid_counter++;
		proc->pid = -kpid_counter;
		proc->is_kernel = true;
		proc->regs.cs = GDT_SEGMENT_CODE_RING0 | 0;
		proc->regs.ss = GDT_SEGMENT_DATA_RING0 | 0;
	} else {
		pid_counter++;
		proc->pid = pid_counter;
		proc->is_kernel = false;
		proc->regs.cs = GDT_SEGMENT_CODE_RING3 | 3;
		proc->regs.ss = GDT_SEGMENT_DATA_RING3 | 3;
	}

	proc->state = PROC_RUNNING; /* prevent immediate scheduling */
	struct rbnode *proc_node = rbt_insert(proc_tree, proc->pid);
	proc_node->value = (uintptr_t)proc;

	spinlock_release(&proc->lock);

	return proc;
}

struct proc *proc_create()
{
	return proc_createv(PT_USER);
}

struct proc *proc_get(pid_t pid)
{
	struct rbnode *proc_node = rbt_search(proc_tree, pid);

	if (proc_node)
		return (void *)proc_node->value;
	else
		return NULL;
}

void proc_enter_kernel(uint64_t save, void *ret)
{
	struct proc *proc = proc_find(getpid());

	if (proc->is_kernel) {
		kprintf(LOG_ERROR "proc: proc_enter_kernel: process %d is already a kernel process\n", getpid());
		panic();
	}

	struct proc *kernel_proc;
	if (proc->buddy_proc) {
		kernel_proc = proc->buddy_proc;
	} else {
		kernel_proc = proc_createv(PT_KERN);

		kernel_proc->is_kernel = true;
		kernel_proc->buddy_proc = proc;
	}

	proc_init_memory(kernel_proc, PAGE_PRESENT | PAGE_RW);

	spinlock_acquire(&proc->lock);
	proc->state = PROC_BLOCKED;
	spinlock_release(&proc->lock);

	proc->buddy_proc = kernel_proc;
	kernel_proc->buddy_proc = proc;

	/* move the save value into RAX */
	kernel_proc->regs.rax = save;
	proc_set_current(kernel_proc->pid);
	load_stack_and_jump(kernel_proc->regs.rsp, kernel_proc->regs.rbp, ret, (void *)save);
}

uint64_t proc_leave_kernel(uint64_t ret, bool shouldret)
{
	struct proc *proc = proc_find(getpid());

	if (!proc->is_kernel) {
		kprintf(LOG_ERROR "proc: proc_leave_kernel: process %d is not a kernel process\n", getpid());
		panic();
	}

	struct proc *uproc = proc->buddy_proc;

	if (shouldret) {
		uproc->regs.rax = ret;
	}

	proc_set_current(uproc->pid);

	spinlock_acquire(&proc->lock);
	proc->state = PROC_BLOCKED;
	spinlock_release(&proc->lock);

	spinlock_acquire(&uproc->lock);
	uproc->state = PROC_RUNNING;
	spinlock_release(&uproc->lock);

	return ret;
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
	proc_set_current(0);
	(void)rbt_insert(proc_tree, 0);
}
