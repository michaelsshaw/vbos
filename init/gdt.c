/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/gdt.h>
#include <kernel/common.h>

#include <stdio.h>

void gdt_load();

void gdt_insert(uint16_t offset, uint8_t access, uint8_t flags)
{
	struct gdt_desc *desc = (__gdt + offset);
	desc->limit = 0;
	desc->base = 0;
	desc->base_2 = 0;
	desc->base_3 = 0;

	desc->access = access;
	desc->flags = flags;
}

void gdt_init()
{
	gdt_insert(0, 0, 0);
	gdt_insert(1, GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(2, GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(3, GDT_ACCESS_CODE_RING3, 0xA0);
	gdt_insert(4, GDT_ACCESS_DATA_RING3, 0xC0);
	__gdtr.size = (sizeof(__gdt) - 1);
	__gdtr.offset = (uint64_t)__gdt;
	gdt_load();

	printf(LOG_SUCCESS "GDT initialized\n");
}
