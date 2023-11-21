/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/pio.h>
#include <kernel/proc.h>
#include <dev/pic.h>

void pic_mask(uint8_t irq, uint8_t i)
{
	uint16_t port;
	uint8_t val;

	i = !(!i);

	if (irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}

	val = inb(port) & ~(1 << irq);
	val |= (i << irq);

	outb(port, val);
}

void pic_eoi(uint8_t irq)
{
	if (irq > 8)
		outb(PIC2_CMD, 0x20);

	outb(PIC1_CMD, 0x20);
}

void exception(int vector, int error)
{
	kprintf(LOG_WARN "EXCEPTION: %x, error code: %d\n", vector, error);
	pid_t pid = getpid();

	struct proc *proc = proc_find(pid);

	if (proc->is_kernel) {
		kprintf(LOG_ERROR "Kernel process %d crashed\n", pid);
		panic();
	} else {
		proc_term(pid);
		kprintf(LOG_WARN "Process %d terminated\n", pid);
		yield();
	}
}

void pic_init()
{
	/* disable the PIC */
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}
