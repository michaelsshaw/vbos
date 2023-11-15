/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/rbtree.h>

static slab_t *kmap_slab;
static spinlock_t kmap_lock = 0;

extern void *(*alloc_page)();

static struct rbtree *kmap_tree = NULL;

static void *kmap_alloc_page()
{
	return slab_alloc(kmap_slab);
}

static uint64_t *pagemap_traverse(uint64_t *this_level, size_t next_num)
{
	uint64_t *r;
	if (!(this_level[next_num] & 1)) {
		r = alloc_page();
		memset(r, 0, 4096);
		this_level[next_num] = ((uintptr_t)r | 3) & ~hhdm_start;
	} else {
		r = (void *)((this_level[next_num] & -4096ull) | hhdm_start);
	}
	return r;
}

uintptr_t kmap_find_unmapped(size_t len)
{
	len = MIN(len, 0x1000);
	len = npow2(len);

	spinlock_acquire(&kmap_lock);
	if (kmap_tree == NULL) {
		spinlock_release(&kmap_lock);
		return 0;
	}

	struct rbnode *node = kmap_tree->root;
	if (node == NULL) {
		spinlock_release(&kmap_lock);
		return 0;
	}

	/* find the lowest address that is mapped */
	while (node->left != NULL)
		node = node->left;

	size_t ret = hhdm_start;

	while (node != NULL) {
		/* node value is the physical address, node key is the virtual address 
		 *
		 * we want to find the lowest virtual address that is not mapped
		 */
		if (node->key > ret + len) {
			spinlock_release(&kmap_lock);
			return ret;
		}

		ret = node->key + node->value2;
		node = rbt_successor(node);
	}

	spinlock_release(&kmap_lock);
	return ret;
}

void kmap(paddr_t paddr, uintptr_t vaddr, size_t len, uint64_t attr)
{
	spinlock_acquire(&kmap_lock);
	uint64_t *pdpt;
	uint64_t *pdt;
	uint64_t *pt;

	size_t pdpn;
	size_t pdn;
	size_t ptn;
	size_t pn;

	if (pml4 == NULL) {
		pml4 = alloc_page();
		assert(pml4 != NULL);
		memset(pml4, 0, 0x1000);
	}

	len += paddr & 4095;
	paddr &= -4096ull;
	vaddr &= -4096ull;

	len = (len + 4095) & -4096ull;

	if (kmap_tree != NULL) {
		struct rbnode *node = rbt_insert(kmap_tree, vaddr);

		if (node == NULL) {
			kprintf(LOG_WARN "kmap: Tried to map already mapped address %X\n", vaddr);
			spinlock_release(&kmap_lock);
			return;
		}

		node->value = paddr;
		node->value2 = len;
	}

	for (; len; len -= 4096, vaddr += 4096, paddr += 4096) {
		pn = (vaddr >> 12) & 0x1FF;
		ptn = (vaddr >> 21) & 0x1FF;
		pdn = (vaddr >> 30) & 0x1FF;
		pdpn = (vaddr >> 39) & 0x1FF;

		pdpt = pagemap_traverse((uint64_t *)pml4, pdpn);
		pdt = pagemap_traverse(pdpt, pdn);
		pt = pagemap_traverse(pdt, ptn);
		pt[pn] = paddr | attr;
	}

	/* flush TLB */
	uint64_t cr3 = cr3_read();
	cr3_write(cr3);

	spinlock_release(&kmap_lock);
}

void *kmap_device(void *dev_paddr, size_t len)
{
	paddr_t dev = (paddr_t)dev_paddr;
	paddr_t dev_page = dev & ~(0xFFF);
	uintptr_t vaddr = kmap_find_unmapped(len);
	kmap(dev_page, vaddr, len, PAGE_PCD | PAGE_PWT | PAGE_RW | PAGE_PRESENT);

	vaddr |= (dev & 0xFFF);
	return (void *)vaddr;
}

static bool kunmap_check_table(uint64_t *table, uint64_t *parent, size_t n)
{
	for (size_t i = 0; i < 512; i++) {
		if (table[i] & 1) {
			return false;
		}
	}
	slab_free(kmap_slab, table);

	if (parent != NULL)
		parent[n] = 0;

	return true;
}

void kunmap(uintptr_t vaddr)
{
	spinlock_acquire(&kmap_lock);
	if (kmap_tree == NULL)
		return;

	uint64_t *pdpt;
	uint64_t *pdt;
	uint64_t *pt;

	size_t pdpn;
	size_t pdn;
	size_t ptn;
	size_t pn;

	struct rbnode *node = rbt_search(kmap_tree, vaddr);

	if (node == NULL) {
		kprintf("kunmap: Tried to unmap unmapped address %X\n", vaddr);
		spinlock_release(&kmap_lock);
		return;
	}

	size_t len = node->value2;

	for (; len; len -= 4096, vaddr += 4096) {
		pn = (vaddr >> 12) & 0x1FF;
		ptn = (vaddr >> 21) & 0x1FF;
		pdn = (vaddr >> 30) & 0x1FF;
		pdpn = (vaddr >> 39) & 0x1FF;

		pdpt = pagemap_traverse((uint64_t *)pml4, pdpn);
		pdt = pagemap_traverse(pdpt, pdn);
		pt = pagemap_traverse(pdt, ptn);
		pt[pn] = 0;

		if (!kunmap_check_table(pt, pdt, ptn))
			continue;
		if (!kunmap_check_table(pdt, pdpt, pdn))
			continue;

		kunmap_check_table(pdpt, (uint64_t *)pml4, pdpn);
	}

	rbt_delete(kmap_tree, node);
	spinlock_release(&kmap_lock);
}

void page_init(paddr_t kpaddr, uintptr_t kvaddr, size_t kernel_size, struct mem_region *regions, size_t num_regions)
{
	kmap_slab = slab_create(0x1000, 4 * MB, SLAB_PAGE_ALIGN);

	/* this inits kmalloc */
	slabtypes_init();

	/* kmalloc is available after this point */
	kmap_tree = kzalloc(sizeof(struct rbtree), ALLOC_KERN);

	/* initialize and populate the new permanent kernel page tables */
	alloc_page = kmap_alloc_page;
	pml4 = NULL;

	/* do not invalidate TLB entries for kernel pages */
	uint64_t attrs = PAGE_PRESENT | PAGE_RW;
	kmap(kpaddr, kvaddr, kernel_size, attrs);

	attrs |= PAGE_XD;
	for (unsigned int i = 0; i < num_regions; i++) {
		kmap(regions[i].base, regions[i].base | hhdm_start, regions[i].len, attrs);
	}

	paddr_t pml4_paddr = (uintptr_t)pml4;
	pml4_paddr &= (~hhdm_start);

	kcr3 = pml4_paddr;

	cr3_write(kcr3);

	/* done */
	kprintf(LOG_SUCCESS "Paging initialized\n");
}
