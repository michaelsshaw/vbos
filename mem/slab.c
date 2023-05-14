/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>

#define SLAB_ALIGN 7

static size_t slab_sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
static struct slab *slab_cache[ARRAY_SIZE(slab_sizes)];
static size_t dma_sizes[] = { 0x1000, 0x10000, 0x100000 };
static struct slab *slab_cache_dma[ARRAY_SIZE(slab_sizes)];

static struct rbtree kmalloc_tree = { NULL };

static inline void slab_range(struct slab *slab, uintptr_t *o_start, uintptr_t *o_end)
{
	uintptr_t sslab = (uintptr_t)slab;
	sslab = (sslab | slab->align) + 1;

	*o_start = sslab;
	*o_end = sslab + (slab->size * slab->num);
}

struct slab *slab_create(size_t size, size_t cache_size, uint64_t flags)
{
	if (size < sizeof(uintptr_t))
		return NULL;

	struct slab *ret = buddy_alloc(cache_size);
	if (ret == NULL)
		return NULL;

	cache_size = npow2(cache_size);

	/* slab area start position */

	uintptr_t aret = (uintptr_t)ret;
	uintptr_t start = (uintptr_t)ret + sizeof(struct slab);

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

	uintptr_t *ptr = (uintptr_t *)start;
	for (size_t i = 0; i < ret->num; i++) {
		*ptr = (uintptr_t)ptr + size;
		if (i == ret->num - 1)
			*ptr = 0;
		ptr = (uintptr_t *)*ptr;
	}

	return ret;
}

void *slab_alloc(struct slab *slab)
{
	if (slab == NULL)
		return NULL;

	if (slab->nextfree == NULL) {
		if (slab->next == NULL) {
			slab->next = slab_create(slab->size, slab->tsize, slab->flags);
			if (slab->next == NULL)
				return NULL;
			slab->next->prev = slab;
		}
		return slab_alloc(slab->next);
	}

	slab->free--;
	uintptr_t *ret = slab->nextfree;
	slab->nextfree = (uintptr_t *)*slab->nextfree;

	return ret;
}

void slab_free(struct slab *slab, void *ptr)
{
	if (slab == NULL)
		return;
	if (ptr == NULL)
		return;

	uintptr_t low = 0;
	uintptr_t high = 0;

	slab_range(slab, &low, &high);
	while ((uintptr_t)ptr < low || (uintptr_t)ptr > high) {
		slab_range(slab, &low, &high);
		slab = slab->next;
		if (slab == NULL)
			return;
	}

	*(uintptr_t *)ptr = (uintptr_t)slab->nextfree;
	slab->nextfree = ptr;
	slab->free++;

	/* delete the slab if it's empty and not a root slab */
	if (slab->free == slab->num && slab->prev != NULL) {
		struct slab *next = slab->next;
		struct slab *prev = slab->prev;

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
}

void kmalloc_init()
{
	for (size_t i = 0; i < ARRAY_SIZE(slab_sizes); i++)
		slab_cache[i] = slab_create(slab_sizes[i], 128 * KB, 0);
	for (size_t i = 0; i < ARRAY_SIZE(dma_sizes); i++)
		slab_cache_dma[i] = slab_create(dma_sizes[i], 8 * MB, SLAB_DMA_64K);
}

static void *kmalloc_table(size_t size, size_t *sizes, size_t nsizes, struct slab **cache, bool fb_disable)
{
	struct slab *slab = NULL;
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

	return ret;
}

void *kmalloc(size_t size, uint64_t flags)
{
	void *ret = NULL;

	if (flags & ALLOC_DMA) {
		ret = kmalloc_table(size, dma_sizes, ARRAY_SIZE(dma_sizes), slab_cache_dma, true);
	} else {
		ret = kmalloc_table(size, slab_sizes, ARRAY_SIZE(slab_sizes), slab_cache, false);
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

void kfree(void *ptr)
{
	if (ptr == NULL)
		return;

	struct rbnode *n = rbt_search(&kmalloc_tree, (uint64_t)ptr);
	if (n == NULL) {
		kprintf(LOG_ERROR "kmalloc: invalid free: %X\n", ptr);
		return;
	}

	struct slab *slab = (struct slab *)n->value;
	if (slab != NULL)
		slab_free(slab, ptr);
	else
		buddy_free(ptr);

	rbt_delete(&kmalloc_tree, n);
}

void slabtypes_init()
{
	rbt_slab_init();

	/* LAST! */
	kmalloc_init();
}
