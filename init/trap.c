/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <kernel/proc.h>

static const char *exception_names[] = {
	"Divide-by-zero Error",
	"Debug",
	"Non-maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved",
	"x87 Floating-Point Exception",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point Exception",
	"Virtualization Exception",
	"Invalid exception",
	"Invalid exception",
	"Invalid exception",
	"Invalid exception",
	"Invalid exception",
	"Invalid exception",
	"Invalid exception",
	"Security Exception",
	"Reserved",
	"Triple Fault",
};

void exception(int vector, int error)
{
	pid_t pid = getpid();

	struct proc *proc = proc_find(pid);

	if (proc->is_kernel) {
		kprintf(LOG_ERROR "Kernel process %d crashed: %s(%xh)\n", pid, exception_names[vector], error);
		panic();
	} else {
		proc_term(pid);
		kprintf("Process %d terminated: %s(%xh)\n", pid, exception_names[vector], error);
		proc_set_current(0);

		schedule();
	}
}
