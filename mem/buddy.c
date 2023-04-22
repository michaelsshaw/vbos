/* SPDX-License-Identifier: GPL-2.0-only */
#include <limine/limine.h>

#include <kernel/common.h>
#include <kernel/mem.h>

struct limine_memmap_request map_req = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };
struct limine_kernel_address_request kern_req = { .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0 };

static char *kmem;
static size_t kmem_len;

struct page *pml4;

struct mem_region *mem_regions;
size_t num_regions;

static void *(*alloc_page)(void);

static void buddy_bitmap_set(char *bitmap, size_t depth, size_t n, bool b)
{
	size_t a = 1 << depth;
	a -= 1;

	if (depth == 0) {
		a = 0;
		n = 0;
	}

	if (b)
		bitmap_set(bitmap, n + a);
	else
		bitmap_clear(bitmap, n + a);
}

static uint8_t buddy_bitmap_get(char *bitmap, size_t depth, size_t n)
{
	size_t a = 1 << depth;
	a -= 1;

	if (depth == 0) {
		a = 0;
		n = 0;
	}

	return bitmap_get(bitmap, n + a);
}

static void buddy_init_region(struct mem_region *region)
{
	/* 2 bits per page */
	if(!region->usable)
		return;

	size_t bitmap_size_bytes = (region->len >> 14) + 1;

	struct buddy_region_header *head = (void *)(region->base | hhdm_start);
	memset(head->bitmap, 0, bitmap_size_bytes);

	paddr_t usable_end = region->base + region->len;
	paddr_t curpos = region->base + sizeof(struct buddy_region_header) + bitmap_size_bytes;
	/* align to 4K */
	curpos |= 0xFFF;
	curpos += 1;

	head->usable_base = curpos;
	head->usable_len = (npow2(usable_end - curpos) >> 1);

	struct buddy_free_list *flist = (struct buddy_free_list *)(head->usable_base | hhdm_start);
	flist->next = NULL;
	flist->prev = NULL;
	flist->len = head->usable_len;
	flist->depth = 0;

	head->flist = flist;
}

static paddr_t buddy_get_slab(struct buddy_region_header *head, size_t depth, size_t n)
{
	if (depth == 0) {
		return head->usable_base;
	}

	return head->usable_base + (n * (head->usable_len >> depth));
}

/* The buddy allocator checks within a pre-defined power-of-2 free-list for a
 * region of the requested size. If the region is the correct size, it allocates
 * it, and if it finds one that is too small, it skips it. If the region found 
 * is too large, instead of skipping to the next, the allocator splits the 
 * region in half recursively until it is of the correct size, and marks the 
 * bitmap accordingly. 
 */

static paddr_t buddy_alloc_helper(struct buddy_region_header *head, size_t size)
{
	if (npow2(size) != size) {
		printf(LOG_WARN "buddy_alloc: size not a power of 2\n");
	}

	if (size < 0x1000) {
		printf(LOG_WARN "buddy_alloc: size too small\n");
		return 0;
	}

	struct buddy_free_list *flist = head->flist;

	while (flist != NULL) {
		if (flist->len > size) {
			/* split the region into two */
			struct buddy_free_list *new = (struct buddy_free_list *)((uintptr_t)flist + flist->len / 2);
			new->next = flist->next;
			new->prev = flist;
			new->len = MAX(flist->len >> 1, 0x1000ull);
			new->depth = flist->depth + 1;

			if (new->next != NULL)
				new->next->prev = new;

			/* Mark the bitmap */
			size_t n = ((uintptr_t)flist ^ hhdm_start) - head->usable_base;
			n /= flist->len;

			buddy_bitmap_set(head->bitmap, flist->depth, n, true);

			/* modify the original region */
			flist->next = new;
			flist->len = flist->len >> 1;
			flist->depth = flist->depth + 1;

		} else if (flist->len == size) {
			/* remove the region from the free list */

			if (flist->next != NULL) /* must be done first! */
				flist->next->prev = flist->prev;

			if (flist->prev != NULL)
				flist->prev->next = flist->next;
			else
				head->flist = flist->next;

			size_t n = ((uintptr_t)flist ^ hhdm_start) - head->usable_base;
			n /= flist->len;

			buddy_bitmap_set(head->bitmap, flist->depth, n, true);

			return ((paddr_t)flist ^ hhdm_start);
		} else {
			flist = flist->next;
		}
	}

	return 0;
}

/* The free function uses the bitmap to determine the size of the region that
 * is going to be freed, and then merges it with its buddy if it is free. For
 * all other operations, the freelist is used instead of the bitmap
 *
 * The bitmap for this allocator is 2 bits per page within a region.
 * It begins with one bit representing the entire region, then 2 bits 
 * representing that region split into two, then 4 bits representing that region
 * split into 4, and so on, until a granularity of 4K is reached
 *
 * Conceptually, the bitmap is a tree, where each node has two children, and is
 * laid out flat in memory.
 *
 * After marking the bitmap and coalescing the regions, the free function
 * searches the free-list for the correct position to insert the region, and
 * inserts it.
 */
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
	buddy_bitmap_set(head->bitmap, depth, n, false);

	/* merge with adjacent regions */
	while (depth > 0 && !buddy_bitmap_get(head->bitmap, depth, n ^ 1)) {
		n >>= 1;
		depth--;

		buddy_bitmap_set(head->bitmap, depth, n, false);
	}

	/* add the region to the free list */
	/* TODO: Replace with binary tree traversal */
	struct buddy_free_list *flist = (struct buddy_free_list *)(buddy_get_slab(head, depth, n) | hhdm_start);
	flist->len = head->usable_len >> depth;
	flist->depth = depth;

	flist->next = NULL;
	flist->prev = NULL;

	if (head->flist == NULL) {
		head->flist = flist;
	} else if (head->flist > flist) {
		flist->next = head->flist;
		head->flist->prev = flist;
		head->flist = flist;
	} else {
		struct buddy_free_list *cur = head->flist;

		while (cur != NULL) {
			if (cur < flist && (cur->next > flist || cur->next == NULL)) {
				flist->next = cur->next;
				flist->prev = cur;
				cur->next = flist;

				if (cur->next != NULL)
					cur->next->prev = flist;

				break;
			}
			cur = cur->next;
		}
	}
}

void *buddy_alloc(size_t size)
{
	if (size == 0)
		return NULL;

	/* Linear search because array is small */
	for (size_t i = 0; i < num_regions; i++) {
		if (!mem_regions[num_regions - i - 1].usable)
			continue;

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

	paddr_t paddr = (paddr_t)ptr ^ hhdm_start;

	/* Linear search because array is small */
	for (size_t i = 0; i < num_regions; i++) {
		if (!mem_regions[num_regions - i - 1].usable)
			continue;

		struct buddy_region_header *head = (void *)(mem_regions[num_regions - i - 1].base | hhdm_start);

		if (paddr >= head->usable_base && paddr < (head->usable_base + head->usable_len)) {
			buddy_free_helper(head, paddr);
			return;
		}
	}
}

static void *buddy_alloc_page()
{
	return buddy_alloc(0x1000);
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
		r = alloc_page_early();
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

void mem_early_init(char *mem, size_t len)
{
	alloc_page = alloc_page_early;

	kmem = mem;
	kmem_len = len;

	size_t kernel_size = 0;

	bool last_usable = false;
	struct mem_region regions[64];
	size_t nregions = 0;

	/* Parse limine physical memory map and build a list of usable regions */
	struct limine_memmap_response *res = map_req.response;
	for (unsigned int i = 0; i < res->entry_count; i++) {
		struct limine_memmap_entry *entry = res->entries[i];

		switch (entry->type) {
		case LIMINE_MEMMAP_KERNEL_AND_MODULES:
			kernel_size = entry->length;
			last_usable = false;
			break;
		case LIMINE_MEMMAP_USABLE:
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
			if (last_usable) {
				regions[nregions - 1].len += entry->length;
			} else {
				regions[nregions].base = entry->base;
				regions[nregions].len = entry->length;
				regions[nregions].usable = (entry->type != LIMINE_MEMMAP_KERNEL_AND_MODULES);
				nregions++;
			}

			last_usable = true;
			break;
		default:
			if (!last_usable) {
				regions[nregions - 1].len += entry->length;
			} else {
				regions[nregions].base = entry->base;
				regions[nregions].len = entry->length;
				regions[nregions].usable = false;
				nregions++;
			}

			last_usable = false;
			break;
		}
	}

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
	for (unsigned int i = 0; i < nregions; i++) {
		kmap(regions[i].base, regions[i].base | hhdm_start, regions[i].len, attrs.val);
	}

	/* Switch to the temporary page tables */
	uintptr_t pml4_paddr = (uintptr_t)pml4;
	pml4_paddr ^= hhdm_start;
	struct cr3 cr3 = { .pml4_addr = pml4_paddr >> 12 };
	cr3_write(cr3.val);

	/* Initialize the new regions and allocate space for the permanent 
	 * regions table
	 */
	bool page_found = false;
	for (unsigned int i = 0; i < nregions; i++) {
		if(!regions[i].usable)
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
	num_regions = nregions;
	memcpy(mem_regions, regions, sizeof(struct mem_region) * nregions);

	/* initialize and populate the new permanent kernel page tables 
	 *
	 * yes, this does involve repeating code
	 */
	alloc_page = buddy_alloc_page;
	pml4 = NULL;

	kmap(kpaddr, kvaddr, kernel_size, attrs.val);
	for (unsigned int i = 0; i < nregions; i++) {
		kmap(regions[i].base, regions[i].base | hhdm_start, regions[i].len, attrs.val);
	}
	kmap(0, hhdm_start, 0x100000, attrs.val);

	pml4_paddr = (uintptr_t)pml4;
	pml4_paddr ^= hhdm_start;
	cr3.pml4_addr = pml4_paddr >> 12;
	cr3_write(cr3.val);

	/* done */
	printf(LOG_SUCCESS "Early memory map initialized\n");
}
