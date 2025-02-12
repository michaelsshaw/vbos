/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>
#include <kernel/proc.h>

#define SLAB_ALIGN 7

#define KMT_DMA 0xDE11A000

static size_t slab_sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
static slab_t *kslab_cache[ARRAY_SIZE(slab_sizes)];
static size_t dma_sizes[] = { 0x1000, 0x10000, 0x100000, 0x1000000 };
static slab_t *slab_cache_dma[ARRAY_SIZE(slab_sizes)];
static size_t uslab_sizes[] = { 0x1000, 0x4000, 0x8000, 0x10000 };
static slab_t *uslab_cache[ARRAY_SIZE(slab_sizes)];

static struct rbtree kmalloc_tree = { NULL, 0, 0 };
static struct rbtree umalloc_tree = { NULL, 0, 0 };

static inline void slab_range(slab_t *slab, uintptr_t *o_start, uintptr_t *o_end)
{
	uintptr_t sslab = (uintptr_t)slab;
	sslab = (sslab | slab->align) + 1;

	*o_start = sslab;
	*o_end = sslab + (slab->size * slab->num);
}

slab_t *slab_create(size_t size, size_t cache_size, uint64_t flags)
{
	if (size < sizeof(uintptr_t))
		return NULL;

	slab_t *ret = buddy_alloc(cache_size);
	if (ret == NULL)
		return NULL;

	cache_size = npow2(cache_size);

	/* slab area start position */

	uintptr_t aret = (uintptr_t)ret;
	uintptr_t start = (uintptr_t)ret + sizeof(slab_t);

	if (flags & SLAB_PAGE_ALIGN) {
		ret->align = 0xFFF;
	} else if (flags & SLAB_DMA_64K) {
		ret->align = 0xFFFF;
		struct page attrs = kdefault_attrs;
		attrs.pcd = 1;
		kmap((paddr_t)(ret) & (~hhdm_start), (paddr_t)ret, cache_size, attrs.val);
	} else {
		ret->align = SLAB_ALIGN;
	}

	start -= 1;
	start |= ret->align;
	start += 1;

	ret->num = (((aret + cache_size) - start) / size);
	ret->size = size;
	ret->tsize = cache_size;
	ret->next = NULL;
	ret->prev = NULL;
	ret->flags = flags;
	ret->nextfree = (uintptr_t *)start;
	ret->free = ret->num;
	ret->lock = 0;

	uintptr_t *ptr = (uintptr_t *)start;
	for (size_t i = 0; i < ret->num; i++) {
		*ptr = (uintptr_t)ptr + size;
		if (i == ret->num - 1)
			*ptr = 0;
		ptr = (uintptr_t *)*ptr;
	}

	return ret;
}

void *slab_alloc(slab_t *slab)
{
	if (slab == NULL)
		return NULL;

	spinlock_t *lock = &slab->lock;
	spinlock_acquire(lock);
	if (slab->nextfree == NULL) {
		if (slab->next == NULL) {
			slab->next = slab_create(slab->size, slab->tsize, slab->flags);
			if (slab->next == NULL) {
				spinlock_release(lock);
				return NULL;
			}
			slab->next->prev = slab;
		}

		void *ret = slab_alloc(slab->next);
		spinlock_release(lock);

		return ret;
	}

	slab->free--;
	uintptr_t *ret = slab->nextfree;
	slab->nextfree = (uintptr_t *)*slab->nextfree;

	spinlock_release(lock);

	return ret;
}

void slab_free(slab_t *slab, void *ptr)
{
	if (slab == NULL)
		return;
	if (ptr == NULL)
		return;

	uintptr_t low = 0;
	uintptr_t high = 0;

	spinlock_t *lock = &slab->lock;
	spinlock_acquire(lock);

	slab_range(slab, &low, &high);
	while ((uintptr_t)ptr < low || (uintptr_t)ptr > high) {
		slab_range(slab, &low, &high);
		slab = slab->next;
		if (slab == NULL) {
			spinlock_release(lock);
			return;
		}
	}

	*(uintptr_t *)ptr = (uintptr_t)slab->nextfree;
	slab->nextfree = ptr;
	slab->free++;

	/* delete the slab if it's empty and not a root slab */
	if (slab->free == slab->num && slab->prev != NULL) {
		slab_t *next = slab->next;
		slab_t *prev = slab->prev;

		prev->next = next;
		if (next != NULL)
			next->prev = prev;

		if (slab->flags & SLAB_DMA_64K) {
			struct page attrs = kdefault_attrs;
			attrs.pcd = 0;
			kmap((paddr_t)slab & (~hhdm_start), (paddr_t)slab, slab->tsize, attrs.val);
		}

		buddy_free(slab);
	}

	spinlock_release(lock);
}

void kmalloc_init()
{
	for (size_t i = 0; i < ARRAY_SIZE(slab_sizes); i++)
		kslab_cache[i] = slab_create(slab_sizes[i], 128 * KB, 0);
	for (size_t i = 0; i < ARRAY_SIZE(dma_sizes); i++)
		slab_cache_dma[i] = slab_create(dma_sizes[i], 8 * MB, SLAB_DMA_64K);
}

static void *kmalloc_table(size_t size, size_t *sizes, size_t nsizes, slab_t **cache, bool fb_disable)
{
	slab_t *slab = NULL;
	void *ret = NULL;
	if (size > sizes[nsizes - 1] && !fb_disable) {
		ret = buddy_alloc(npow2(size));
	} else {
		for (size_t i = 0; i < nsizes; i++) {
			if (size <= sizes[i]) {
				size = sizes[i];
				slab = cache[i];
				ret = slab_alloc(slab);
				break;
			}
		}
	}

	if (ret == NULL)
		return NULL;

	struct rbnode *n = rbt_insert(&kmalloc_tree, (uint64_t)ret);
	if (n == NULL) {
		if (slab != NULL)
			slab_free(slab, ret);
		else
			buddy_free(ret);
		return NULL;
	}
	n->value = (uint64_t)slab;
	n->value2 = size;

	return ret;
}

void *kmalloc(size_t size, uint64_t flags)
{
	void *ret = NULL;

	if (flags & ALLOC_DMA) {
		ret = kmalloc_table(size, dma_sizes, ARRAY_SIZE(dma_sizes), slab_cache_dma, true);
		if (ret == NULL) {
			ret = buddy_alloc(npow2(size));
			if (ret == NULL)
				return NULL;

			struct rbnode *n = rbt_insert(&kmalloc_tree, (uint64_t)ret);
			if (n == NULL) {
				buddy_free(ret);
				return NULL;
			}

			n->value = 0;
			n->value2 = size;
			n->value3 = KMT_DMA;
		}

	} else {
		ret = kmalloc_table(size, slab_sizes, ARRAY_SIZE(slab_sizes), kslab_cache, false);
	}

	return ret;
}

void *kzalloc(size_t size, uint64_t flags)
{
	void *ret = kmalloc(size, flags);
	if (ret == NULL)
		return NULL;

	memset(ret, 0, size);
	return ret;
}

void *krealloc(void *ptr, size_t size, uint64_t flags)
{
	/* invalid case 1: ptr is NULL */
	if (ptr == NULL)
		return kmalloc(size, flags);

	/* invalid case 2: size is 0 */
	if (size == 0) {
		return ptr;
	}

	/* invalid case 3: ptr is not allocated by kmalloc */
	struct rbnode *n = rbt_search(&kmalloc_tree, (uint64_t)ptr);
	if (n == NULL) {
		kprintf(LOG_ERROR "kmalloc: invalid realloc: %X\n", ptr);
		return NULL;
	}

	/* invalid case 4: size is smaller than the original size */
	size_t old_size = n->value2;
	if (size <= old_size)
		return ptr;

	/* valid case */
	void *ret = kmalloc(size, flags);
	if (ret == NULL)
		return NULL;

	memcpy(ret, ptr, old_size);
	kfree(ptr);

	return ret;
}

void kfree(void *ptr)
{
	if (ptr == NULL)
		return;

	struct rbnode *n = rbt_search(&kmalloc_tree, (uint64_t)ptr);
	if (n == NULL) {
		kprintf(LOG_ERROR "kmalloc: invalid free: %X\n", ptr);
		return;
	}

	slab_t *slab = (slab_t *)n->value;
	if (slab != NULL) {
		slab_free(slab, ptr);
	} else {
		buddy_free(ptr);

		if (n->value3 == KMT_DMA) {
			uintptr_t p = (uintptr_t)ptr;
			kmap(p & (~hhdm_start), p, n->value2, kdefault_attrs.val);
		}
	}

	rbt_delete(&kmalloc_tree, n);
}

void ufree(struct proc *proc, void *addr)
{
}

void *umalloc(struct proc *proc, size_t size, uint64_t opts)
{
	uint64_t flags = 0;

	struct proc_allocation *a = kzalloc(sizeof(struct proc_allocation), 0);

	if (a == NULL)
		return NULL;

	const size_t max_slab_size = uslab_sizes[ARRAY_SIZE(uslab_sizes) - 1];
	uintptr_t vaddr = mmap_find_unmapped(&proc->page_map, &proc->page_map_lock, USER_HEAP_BASE, size);
	uint64_t attr;

	attr = PAGE_USER | PAGE_PRESENT;

	if (!(opts & ALLOC_USER_RDONLY))
		attr |= PAGE_RW;

	if (!(opts & ALLOC_USER_EXEC))
		attr |= PAGE_XD;

	if (vaddr == 0)
		return NULL;

	/* Do the allocation
	 *
	 * This part of the allocator decides whether to chain slabs, or to use a buddy allocator
	 *
	 * Slab chaining is only used in userspace allocations, and this is the only allocator that
	 * does it.
	 */
	if (size > max_slab_size * 256) {
		/* Case 1: 
		 * Huge allocation: Just do a buddy_alloc 
		 */
		struct proc_mapping *m = kzalloc(sizeof(struct proc_mapping), 0);
		if (m == NULL) {
			ufree(proc, a);
			return NULL;
		}
		size = npow2(size);
		void *ret = buddy_alloc(size);

		if (ret == NULL)
			return NULL;

		a->mappings = m;
		a->num_mappings = 1;

		m->vaddr = vaddr;
		m->paddr = ((paddr_t)(ret) & (~hhdm_start));
		m->len = size;
		m->type = PM_BUD;
		m->attr = attr;

		proc_mmap(proc, m->paddr, vaddr, size, m->attr);

		struct rbnode *n = rbt_insert(&umalloc_tree, (uint64_t)vaddr);
		n->value = (uint64_t)a;
	} else {
		/* Case 2: 
		 * Regular sized allocation: Use slabs 
		 */
		size_t total_alloc = 0;
		while (total_alloc < size) {
			struct proc_mapping *m = kzalloc(sizeof(struct proc_mapping), 0);

			if (m == NULL) {
				ufree(proc, a);
				return NULL;
			}

			if (a->num_mappings == 0)
				a->mappings = m;
			else
				a->last->next = m;

			a->last = m;

			a->num_mappings++;

			size_t remaining = size - total_alloc;
			size_t this_size = uslab_sizes[ARRAY_SIZE(uslab_sizes) - 1];
			size_t this_index = ARRAY_SIZE(uslab_sizes) - 1;
			for (int i = ARRAY_SIZE(uslab_sizes) - 2; i >= 0; i--) {
				if (remaining >= uslab_sizes[i]) {
					this_index = i + 1;
					this_size = uslab_sizes[this_index];
					break;
				}
			}

			void *ret = slab_alloc(uslab_cache[this_index]);
			m->type = PM_SLB;
			m->vaddr = vaddr + total_alloc;
			m->paddr = ((paddr_t)(ret) & (~hhdm_start));
			m->len = this_size;
			m->attr = PAGE_USER | PAGE_PRESENT;
			m->slab = uslab_cache[this_index];

			proc_mmap(proc, m->paddr, m->vaddr, m->len, m->attr);
		}
	}

	return (void *)vaddr;
}

static void umalloc_init()
{
	for (size_t i = 0; i < ARRAY_SIZE(uslab_sizes); i++)
		uslab_cache[i] = slab_create(uslab_sizes[i], 256 * KB, SLAB_PAGE_ALIGN);
}

void slabtypes_init()
{
	rbt_slab_init();

	umalloc_init();

	/* LAST! */
	kmalloc_init();
}
