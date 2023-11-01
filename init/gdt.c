/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/gdt.h>
#include <kernel/common.h>

#include <stdio.h>

void gdt_load();

static int gdt_index = 0;

spinlock_t gdt_lock = 0;

void gdt_insert(uint8_t access, uint8_t flags)
{
	struct gdt_desc *desc = (__gdt + gdt_index++);
	desc->limit = 0;
	desc->base = 0;
	desc->base_2 = 0;
	desc->base_3 = 0;

	desc->access = access;
	desc->flags = flags;
}

uint16_t gdt_insert_tss(uintptr_t rsp0)
{
	spinlock_acquire(&gdt_lock);
	struct gdt_desc_tss *tss = (struct gdt_desc_tss *)((struct gdt_desc *)__gdt + gdt_index);

	memset(tss, 0, sizeof(*tss));
	tss->rsp0 = rsp0;

	gdt_index += 2;

	spinlock_release(&gdt_lock);
	/* return the index of the TSS descriptor */
	return (gdt_index - 2) * sizeof(struct gdt_desc);
}

void gdt_init()
{
	gdt_insert(0, 0);
	gdt_insert(GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(GDT_ACCESS_DATA_RING0, 0xC0);

	__gdtr.size = (sizeof(__gdt) - 1);
	__gdtr.offset = (uint64_t)__gdt;

	gdt_load();

	kprintf(LOG_SUCCESS "GDT initialized\n");
}
