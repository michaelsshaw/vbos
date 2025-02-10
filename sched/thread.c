/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <kernel/common.h>
#include <kernel/proc.h>
#include <kernel/gdt.h>
#include <kernel/slab.h>

void _kthread_exit();

kthread_t *kthread_create()
{
	struct proc *proc = proc_create();
	if (proc == NULL)
		return NULL;

	kthread_t *thread = kzalloc(sizeof(kthread_t), ALLOC_KERN);
	if (thread == NULL) {
		kfree(proc);
		return NULL;
	}

	thread->proc = proc;

	return thread;
}

kthread_t *kthread_current()
{
	struct proc *proc = proc_find(getpid());
	if (proc == NULL)
		return NULL;

	return proc->waiting_on;
}

void kthread_yield()
{
	sswtch();
}

void kthread_exit(uint64_t retval)
{
	kthread_t *thread = kthread_current();
	if (thread == NULL)
		panic();

	thread->proc->state = PROC_BLOCKED;
	thread->done = true;
	thread->retval = retval;

	sswtch();
}

uint64_t kthread_join(kthread_t *thread)
{
	if (thread == NULL)
		return -1;

	while (!thread->done)
		sswtch();

	uint64_t retval = thread->retval;

	proc_term(thread->proc->pid);
	buddy_free((void *)thread->stack);
	kfree(thread);

	return retval;
}

void kthread_call(kthread_t *thread, uint64_t entry, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	thread->stack = (uintptr_t)buddy_alloc(KSTACK_SIZE);
	thread->stack += KSTACK_SIZE - 8;

	thread->proc->regs.rip = entry;
	thread->proc->regs.rdi = arg1;
	thread->proc->regs.rsi = arg2;
	thread->proc->regs.rdx = arg3;
	thread->proc->regs.rcx = arg4;
	thread->proc->regs.r8 = arg5;

	thread->proc->regs.cs = GDT_SEGMENT_CODE_RING0;
	thread->proc->regs.ss = GDT_SEGMENT_DATA_RING0;
	thread->proc->regs.rflags = 0x246;
	thread->proc->regs.rax = 0;
	thread->proc->cr3 = kcr3;

	thread->proc->regs.rsp = thread->stack;
	thread->proc->regs.rbp = thread->stack;

	thread->proc->state = PROC_STOPPED; /* allow scheduling */

	*(uint64_t *)thread->stack = (uint64_t)_kthread_exit;
}
