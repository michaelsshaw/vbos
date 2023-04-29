/* SPDX-License-Identifier: GPL-2.0-only */
#include <limine/limine.h>

#include <kernel/common.h>
#include <kernel/slab.h>
#include <kernel/mem.h>

struct limine_memmap_request map_req = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };
struct limine_kernel_address_request kern_req = { .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0 };

static char *kmem;
static size_t kmem_len;

struct page *pml4;

struct mem_region *mem_regions;
size_t num_regions;

static void *(*alloc_page)(void);
static struct slab *kmap_slab;

static void buddy_bitmap_set(char *bitmap, size_t depth, size_t n, int val)
{
	size_t a = 2 << depth;
	a -= 2;

	if (depth == 0) {
		a = 0;
		n = 0;
	}
	n <<= 1;

	bitmap_set(bitmap, n + a, val & 0x02);
	bitmap_set(bitmap, n + a + 1, val & 0x01);
}

static uint8_t buddy_bitmap_get(char *bitmap, size_t depth, size_t n)
{
	size_t a = 2 << depth;
	a -= 2;

	if (depth == 0) {
		a = 0;
		n = 0;
	}
	n <<= 1;

	uint8_t ret = 0;
	if (bitmap_get(bitmap, n + a))
		ret |= 0x02;
	if (bitmap_get(bitmap, n + a + 1))
		ret |= 0x01;
	return ret;
}

static void buddy_init_region(struct mem_region *region)
{
	/* 2 bits per page */
	if (!region->usable)
		return;

	size_t bitmap_size_bytes = (region->len >> 13) + 1;

	struct buddy_region_header *head = (void *)(region->base | hhdm_start);
	memset(head->bitmap, 0, bitmap_size_bytes);

	paddr_t usable_end = region->base + region->len;
	paddr_t curpos = region->base + sizeof(struct buddy_region_header) + bitmap_size_bytes;
	/* align to 4K */
	curpos |= 0xFFF;
	curpos += 1;

	head->usable_base = curpos;
	head->usable_len = (npow2(usable_end - curpos) >> 1);
	head->max_depth = log2(head->usable_len >> 12);
}

static paddr_t buddy_get_slab(struct buddy_region_header *head, size_t depth, size_t n)
{
	if (depth == 0) {
		return head->usable_base;
	}

	return head->usable_base + (n * (head->usable_len >> depth));
}

static paddr_t buddy_alloc_traverse(struct buddy_region_header *head, size_t depth, size_t n, size_t tgt_depth)
{
	if (depth > tgt_depth)
		return 0;
	if (tgt_depth > head->max_depth)
		return 0;

	uint8_t val = buddy_bitmap_get(head->bitmap, depth, n);

	switch (val) {
	case BBMAP_USED:
		if (n & 1)
			return 0;
		else
			return buddy_alloc_traverse(head, depth, n + 1, tgt_depth);

		break;
	case BBMAP_SPLIT:
		if (depth == tgt_depth) {
			if (n & 1)
				return 0;
			else
				return buddy_alloc_traverse(head, depth, n + 1, tgt_depth);
		} else {
			paddr_t ret = buddy_alloc_traverse(head, depth + 1, n << 1, tgt_depth);
			if (ret)
				return ret;
			else
				return buddy_alloc_traverse(head, depth + 1, (n << 1) + 1, tgt_depth);
		}
		break;
	case BBMAP_FREE:
		if (depth == tgt_depth) {
			buddy_bitmap_set(head->bitmap, depth, n, BBMAP_USED);
			return buddy_get_slab(head, depth, n);
		} else {
			buddy_bitmap_set(head->bitmap, depth, n, BBMAP_SPLIT);
			return buddy_alloc_traverse(head, depth + 1, n << 1, tgt_depth);
		}

		break;
	}

	return 0;
}

static paddr_t buddy_alloc_helper(struct buddy_region_header *head, size_t size)
{
	if (npow2(size) != size) {
		printf(LOG_WARN "buddy_alloc: size not a power of 2\n");
	}

	if (size < 0x1000) {
		printf(LOG_WARN "buddy_alloc: size too small\n");
		return 0;
	}

	size_t tgt_depth = head->max_depth - log2(size >> 12);

	return buddy_alloc_traverse(head, 0, 0, tgt_depth);
}

static void buddy_free_helper(struct buddy_region_header *head, paddr_t paddr)
{
	/* find the region in the bitmap */
	paddr_t offset = paddr - head->usable_base;
	size_t curlen = head->usable_len;
	size_t depth = 0;
	size_t n = 0;

	size_t lastn = 0;
	size_t lastdepth = 0;

	while (buddy_bitmap_get(head->bitmap, depth, n)) {
		lastn = n;
		lastdepth = depth;

		depth++;
		n <<= 1;
		curlen >>= 1;

		if (offset >= curlen) {
			offset -= curlen;
			n++;
		}
	}

	n = lastn;
	depth = lastdepth;

	/* mark the region as free */
	buddy_bitmap_set(head->bitmap, depth, n, BBMAP_FREE);

	/* merge with adjacent regions */
	while (depth > 0 && (buddy_bitmap_get(head->bitmap, depth, n ^ 1) == BBMAP_FREE)) {
		n >>= 1;
		depth--;

		buddy_bitmap_set(head->bitmap, depth, n, BBMAP_FREE);
	}
}

void *buddy_alloc(size_t size)
{
	if (size == 0)
		return NULL;

	/* Linear search because array is small */
	for (size_t i = 0; i < num_regions; i++) {
		struct buddy_region_header *head = (void *)(mem_regions[num_regions - i - 1].base | hhdm_start);

		paddr_t ret = buddy_alloc_helper(head, size);
		if (ret != 0)
			return (void *)(ret | hhdm_start);
	}

	return NULL;
}

void buddy_free(void *ptr)
{
	if (ptr == NULL)
		return;

	paddr_t paddr = (paddr_t)ptr & ~hhdm_start;

	/* Linear search because array is small */
	for (size_t i = 0; i < num_regions; i++) {
		struct buddy_region_header *head = (void *)(mem_regions[num_regions - i - 1].base | hhdm_start);

		if (paddr >= head->usable_base && paddr < (head->usable_base + head->usable_len)) {
			buddy_free_helper(head, paddr);
			return;
		}
	}
}

static void *kmap_alloc_page()
{
	return slab_alloc(kmap_slab);
}

static void *alloc_page_early()
{
	static uintptr_t hwm = 0;
	void *r = (void *)(hwm + kmem);
	if (hwm >= kmem_len)
		printf(LOG_ERROR "Out of early kmem\n");
	hwm += 0x1000;
	return r;
}

static uint64_t *next_level(uint64_t *this_level, size_t next_num)
{
	uint64_t *r;
	if (!(this_level[next_num] & 1)) {
		r = alloc_page();
		memset(r, 0, 4096);
		this_level[next_num] = ((uintptr_t)r | 3) - hhdm_start;
	} else {
		r = (void *)((this_level[next_num] & -4096ull) | hhdm_start);
	}
	return r;
}

void kmap(paddr_t paddr, uint64_t vaddr, size_t len, uint64_t attr)
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
		pt[pn] = paddr | attr;
	}
}

static size_t limine_parse_memregions()
{
	size_t ret = 0;
	bool last_usable = false;
	struct limine_memmap_response *res = map_req.response;

	for (unsigned int i = 0; i < res->entry_count; i++) {
		struct limine_memmap_entry *entry = res->entries[i];

		switch (entry->type) {
		case LIMINE_MEMMAP_KERNEL_AND_MODULES:
			ret = entry->length;
			last_usable = false;
			break;
		case LIMINE_MEMMAP_USABLE:
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
			if (last_usable) {
				mem_regions[num_regions - 1].len += entry->length;
			} else {
				mem_regions[num_regions].base = entry->base;
				mem_regions[num_regions].len = entry->length;
				mem_regions[num_regions].usable = (entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES);
				num_regions++;
			}

			last_usable = true;
			break;
		default:
			if (!last_usable) {
				mem_regions[num_regions - 1].len += entry->length;
			} else {
				mem_regions[num_regions].base = entry->base;
				mem_regions[num_regions].len = entry->length;
				mem_regions[num_regions].usable = false;
				num_regions++;
			}

			last_usable = false;
			break;
		}
	}

	return ret;
}

void mem_early_init(char *mem, size_t len)
{
	alloc_page = alloc_page_early;
	kmem = mem;
	kmem_len = len;

	struct mem_region regions[64];
	mem_regions = regions;

	/* Parse limine physical memory map and build a list of usable regions */
	size_t kernel_size = limine_parse_memregions();

	uint64_t kpaddr = kern_req.response->physical_base;
	uint64_t kvaddr = kern_req.response->virtual_base;

	struct page attrs;
	attrs.val = 1;
	attrs.rw = 1;
	attrs.xd = 0;

	/* These page tables are initialized on the kernel stack
	 *
	 * These will be used to initialize the new memory regions we have
	 * parsed from the bootloader, at which point we will switch to page 
	 * tables that are allocated in the new regions.
	 */
	kmap(kpaddr, kvaddr, kernel_size, attrs.val);

	/* Identity map the regions */
	for (unsigned int i = 0; i < num_regions; i++) {
		kmap(regions[i].base, regions[i].base | hhdm_start, regions[i].len, attrs.val);
	}

	/* Switch to the temporary page tables */
	uintptr_t pml4_paddr = (uintptr_t)pml4;
	pml4_paddr &= (~hhdm_start);
	struct cr3 cr3 = { .pml4_addr = pml4_paddr >> 12 };
	cr3_write(cr3.val);

	/* Initialize the new regions and allocate space for the permanent 
	 * regions table
	 */
	bool page_found = false;
	for (unsigned int i = 0; i < num_regions; i++) {
		if (!regions[i].usable)
			continue;

		buddy_init_region(&regions[i]);

		if (!page_found) {
			struct buddy_region_header *head = (struct buddy_region_header *)(regions[i].base | hhdm_start);
			paddr_t r_alloc = buddy_alloc_helper(head, 0x1000);
			if (r_alloc != 0) {
				page_found = true;
				mem_regions = (void *)(r_alloc | hhdm_start);
			}
		}
	}

	/* copy the regions to the permanent regions table */
	memcpy(mem_regions, regions, sizeof(struct mem_region) * num_regions);

	kmap_slab = slab_create(0x1000);

	/* initialize and populate the new permanent kernel page tables */
	alloc_page = kmap_alloc_page;
	pml4 = NULL;

	struct mem_region lastregion = mem_regions[num_regions - 1];
	kmap(kpaddr, kvaddr, kernel_size, attrs.val);

	/* identity map the entire physical address space */
	kmap(0, hhdm_start, lastregion.base + lastregion.len, attrs.val);

	pml4_paddr = (uintptr_t)pml4;
	pml4_paddr &= (~hhdm_start);
	cr3.pml4_addr = pml4_paddr >> 12;
	cr3_write(cr3.val);

	/* done */
	printf(LOG_SUCCESS "Early memory map initialized\n");
}
