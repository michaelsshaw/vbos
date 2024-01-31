/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/gdt.h>
#include <kernel/common.h>
#include <kernel/slab.h>

void gdt_load();

static int gdt_index = 0;
static int tss_index = 0;
static struct tss *tss_table = NULL;

spinlock_t gdt_lock = 0;

void gdt_insert(uint8_t offset, uint8_t access, uint8_t flags)
{
	struct gdt_desc *desc = (__gdt + offset);
	desc->limit = 0;
	desc->base = 0;
	desc->base_2 = 0;
	desc->base_3 = 0;

	desc->access = access;
	desc->flags = flags;
	gdt_index++;
}

uint16_t gdt_insert_tss(uintptr_t rsp0)
{
	spinlock_acquire(&gdt_lock);

	tss_table[tss_index].rsp0 = rsp0;

	struct gdt_desc_sys *desc = (struct gdt_desc_sys *)((struct gdt_desc *)__gdt + gdt_index);

	paddr_t tss_addr = (paddr_t)(tss_table + tss_index);
	desc->limit = sizeof(struct tss) - 1;
	desc->base = tss_addr & 0xFFFFFF;
	desc->base_2 = (tss_addr >> 16) & 0xFF;
	desc->base_3 = (tss_addr >> 24) & 0xFF;
	desc->base_4 = (tss_addr >> 32) & 0xFFFFFFFF;
	desc->access = 0xE9;
	desc->flags = 0x00;

	tss_index += 1;
	gdt_index += 2;

	spinlock_release(&gdt_lock);
	/* return the index of the TSS descriptor */
	return (gdt_index - 2) * sizeof(struct gdt_desc);
}

void gdt_init()
{
	tss_table = kzalloc(sizeof(struct tss) * 128, ALLOC_KERN);

	gdt_insert(0, 0, 0);
	gdt_insert(1, GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(2, GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(3, GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(4, GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(5, GDT_ACCESS_CODE_RING0, 0xA0);
	gdt_insert(6, GDT_ACCESS_DATA_RING0, 0xC0);
	gdt_insert(7, GDT_ACCESS_CODE_RING3, 0xA0);
	gdt_insert(8, GDT_ACCESS_DATA_RING3, 0xC0);

	__gdtr.size = (sizeof(__gdt) - 1);
	__gdtr.offset = (uint64_t)__gdt;

	gdt_load();

	kprintf(LOG_SUCCESS "GDT initialized\n");
}
