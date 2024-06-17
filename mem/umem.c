/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/proc.h>

void copy_to_user(struct proc *proc, void *dst, const void *src, size_t size)
{
	paddr_t ucr3 = proc->cr3;
	paddr_t kcr3 = cr3_read();

	if (kcr3 != ucr3)
		cr3_write(ucr3);

	memcpy(dst, src, size);

	if (kcr3 != ucr3)
		cr3_write(kcr3);
}
