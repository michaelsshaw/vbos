/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/slab.h>
#include <kernel/proc.h>

/* explicit handle list */
void (*exception_handlers[32])(int error) = { NULL };

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

static void exception_page_fault(int err)
{
	uint64_t cr2 = cr2_read();

	kprintf(LOG_ERROR "Page fault at %xh, error code %xh\n", cr2, err);
}

void exception(int vector, int error)
{
	pid_t pid = getpid();

	kprintf("Process %d terminated: %s(%xh)\n", pid, exception_names[vector], error);

	struct proc *proc = proc_find(pid);

	if (exception_handlers[vector])
		exception_handlers[vector](error);

	if(proc->pid == 0)
		panic();

	proc_set_current(0);
	proc_term(pid);
	schedule();
}

void exception_init()
{
	exception_handlers[0x0E] = exception_page_fault;
}
