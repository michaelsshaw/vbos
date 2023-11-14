/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>
#include <dev/apic.h>

static struct rbtree *proc_tree;
static pid_t *proc_current;

void return_to_user(struct procregs *regs, paddr_t cr3);

struct procregs *proc_current_regs()
{
	struct rbnode *proc_node = rbt_search(proc_tree, proc_current[lapic_idno()]);

	if (proc_node) {
		struct proc *ret = (void *)proc_node->value;

		return &ret->regs;
	} else {
		return NULL;
	}
}

void proc_init(unsigned num_cpus)
{
	/* allocate current process structs */
	proc_current = kzalloc(num_cpus * sizeof(pid_t), ALLOC_KERN);
	proc_tree = kzalloc(sizeof(struct rbtree), ALLOC_KERN);

	for (unsigned i = 0; i < num_cpus; i++) {
		proc_current[i] = -1;

		struct proc *proc = kzalloc(sizeof(struct proc), ALLOC_KERN);

		proc->pid = rbt_next_key(proc_tree);
		proc->is_kernel = true;
		proc->cr3 = kcr3;

		struct rbnode *proc_node = rbt_insert(proc_tree, proc->pid);
		proc_node->value = (uintptr_t)proc;

		proc_current[i] = proc->pid;
	}
}
