/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>

#include <dev/apic.h>
#include <limine/limine.h>

struct limine_memmap_request map_req = { .id = LIMINE_MEMMAP_REQUEST,
					 .revision = 0 };

static char *kmem;
static size_t kmem_len;
static uintptr_t hwm = 0;
static struct page *pml4;

static const char *map_names[] = { "USABLE      ", "RESERVED    ",
				   "ACPI_RECLAIM", "ACPI_NVS    ",
				   "BAD_MEM     ", "BOOT_RECLAIM",
				   "KERNEL      ", "FRAMEBUFFER " };

void *alloc_page()
{
	void *r = (void *)(hwm + kmem);
	if (hwm >= kmem_len)
		printf(LOG_ERROR "Out of early kmem\n");
	hwm += 0x1000;
	return r;
}

uint64_t *next_level(uint64_t *this_level, size_t next_num)
{
	uint64_t *r;
	if (!(this_level[next_num] & 1)) {
		r = alloc_page();
		memset(r, 0, 4096);
		this_level[next_num] = (uintptr_t)r | 3;
	} else
		r = (void *)(this_level[next_num] & -4096ull);
	return r;
}

void kmap(uint64_t paddr, uint64_t vaddr, size_t len, uint64_t attr)
{
	uint64_t *pdpt;
	uint64_t *pdt;
	uint64_t *pt;

	size_t pdpn;
	size_t pdn;
	size_t ptn;
	size_t pn;

	if (pml4 == NULL) {
		pml4 = alloc_page();
		memset(pml4, 0, 0x1000);
	}

	len += paddr & 4095;
	paddr &= -4096ull;
	vaddr &= -4096ull;

	len = (len + 4095) & -4096ull;

	for (; len; len -= 4096, vaddr += 4096, paddr += 4096) {
		pn = (vaddr >> 12) & 0x1FF;
		ptn = (vaddr >> 21) & 0x1FF;
		pdn = (vaddr >> 30) & 0x1FF;
		pdpn = (vaddr >> 39) & 0x1FF;

		pdpt = next_level((uint64_t *)pml4, pdpn);
		pdt = next_level(pdpt, pdn);
		pt = next_level(pdt, ptn);
		pt[pn] = paddr | attr | 1;
	}
}

void pfa_init(char *mem, size_t len)
{
	kmem = mem;
	kmem_len = len;

	uint64_t kernel_base = 0;
	size_t kernel_size = 0;

	/* Print limine early memory map */
	struct limine_memmap_response *res = map_req.response;
	for (unsigned int i = 0; i < res->entry_count; i++) {
		struct limine_memmap_entry *entry = res->entries[i];
		printf(LOG_INFO "MEM REGION %s BASE: %X, SIZE: %X\n",
		       map_names[entry->type], entry->base, entry->length);

		switch (entry->type) {
		case LIMINE_MEMMAP_KERNEL_AND_MODULES:
			kernel_base = entry->base;
			kernel_size = entry->length;
			break;
		default:
			break;
		}
	}

	/* Need to make these linker script values usable */

	printf(LOG_WARN "__hhdm_start %X\n", hhdm_start);
	printf(LOG_WARN "__data_end %X\n", data_end);
	printf(LOG_WARN "__bss_start %X\n", bss_start);
	printf(LOG_WARN "__bss_end %X\n", bss_end);
	printf(LOG_WARN "sizeof size_t %d\n", sizeof(size_t));

	struct page attrs;
	attrs.val = 0;
	attrs.rw = 1;
	attrs.xd = 0;
	kmap(kernel_base, hhdm_start, kernel_size, attrs.val);

	struct cr3 cr3 = { .pml4_addr = ((uintptr_t)pml4 >> 12) };
	cr3_write(cr3.val);

	printf(LOG_SUCCESS "Early memory map initialized\n");
}
