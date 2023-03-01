/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>

#include <dev/apic.h>
#include <limine/limine.h>

struct page pml4[512] ALIGN(0x1000);
struct page pdpt[512] ALIGN(0x1000);
struct page pdt[512] ALIGN(0x1000);
struct page pt[512] ALIGN(0x1000);

static void pfa_gen_tables()
{
}

void pfa_init()
{
	uintptr_t pml4_t = (cr3_get() & 0xFFFFFFFFFFFFF000);
	pml4_t += HHDM_START;

	uint64_t *pml4 = (uint64_t *)pml4_t;

	printf(LOG_WARN "PML4 Entry 0: %Xh\n", *pml4);
	printf(LOG_INFO "PML4 ADDRESS: %Xh\n", pml4);
}
