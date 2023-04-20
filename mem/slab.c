/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>

#define SLAB_MIN_COUNT 16

size_t slab_sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
struct slab *slab_cache[ARRAY_SIZE(slab_sizes)];

struct slab *kmap_slab = NULL;
struct kmap_entry *kmem_map = NULL;

void slab_range(struct slab *slab, uintptr_t *o_start, uintptr_t *o_end)
{
	uintptr_t sslab = (uintptr_t)slab;
	sslab = (sslab | 7) + 1;

	*o_start = sslab;
	*o_end = sslab + (slab->size * slab->num);
}

struct slab *slab_create(size_t size)
{
	if (size < sizeof(uintptr_t))
		return NULL;

	/* account for slab header and eight bytes of alignment */
	size_t est = npow2(size * SLAB_MIN_COUNT + sizeof(struct slab) + 8);
	size_t asize = MAX(est, 0x1000ull);

	struct slab *ret = buddy_alloc(asize);
	if (ret == NULL)
		return NULL;

	/* slab area start position */
	size_t start = (sizeof(struct slab) | 7) + 1;

	ret->nextfree = (uintptr_t *)((uintptr_t)ret + start);
	ret->size = size;

	uintptr_t *ptr = ret->nextfree;
	while (ptr != NULL) {
		if (ptr > (uintptr_t *)((uintptr_t)ret + asize - size)) {
			*ptr = (uintptr_t)NULL;
			ptr = NULL;
		} else {
			*ptr = (uintptr_t)ptr + size;
			ptr = (uintptr_t *)*ptr;
		}
	}

	return ret;
}

void *slab_alloc(struct slab *slab)
{
	if (slab == NULL)
		return NULL;

	if (slab->nextfree == NULL) {
		if (slab->next == NULL) {
			slab->next = slab_create(slab->size);
			if (slab->next == NULL)
				return NULL;
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

	uintptr_t low;
	uintptr_t high;
	slab_range(slab, &low, &high);

	if ((uintptr_t)ptr < low || (uintptr_t)ptr > high) {
		slab_free(slab->next, ptr);
		return;
	}

	*(uintptr_t *)ptr = (uintptr_t)slab->nextfree;
	slab->nextfree = ptr;
}

void kmalloc_init()
{
	for (size_t i = 0; i < ARRAY_SIZE(slab_sizes); i++)
		slab_cache[i] = slab_create(slab_sizes[i]);
	kmap_slab = slab_create(sizeof(struct kmap_entry));
}

void *kmalloc(size_t size)
{
	void *ret = NULL;
	struct slab *slab = NULL;

	if (size > slab_sizes[ARRAY_SIZE(slab_sizes) - 1]) {
		ret = buddy_alloc(npow2(size));
	} else {
		for (size_t i = 0; i < ARRAY_SIZE(slab_sizes); i++) {
			if (size <= slab_sizes[i]) {
				size = slab_sizes[i];
				slab = slab_cache[i];
				ret = slab_alloc(slab);

				break;
			}
		}
	}

	if (ret == NULL)
		return NULL;

	struct kmap_entry *entry = slab_alloc(kmap_slab);
	if (entry == NULL) {
		slab_free(slab, ret);
		return NULL;
	}

	entry->ptr = ret;
	entry->slab = slab;
	entry->size = size;
	entry->next = kmem_map;

	kmem_map = entry;

	return ret;
}

void kfree(void *ptr)
{
	if (ptr == NULL)
		return;

	struct kmap_entry *entry = kmem_map;
	struct kmap_entry *prev = NULL;

	while (entry != NULL) {
		if (entry->ptr == ptr) {
			if (prev == NULL)
				kmem_map = entry->next;
			else
				prev->next = entry->next;

			if (entry->slab == NULL)
				buddy_free(entry->ptr);
			else
				slab_free(entry->slab, entry->ptr);

			slab_free(kmap_slab, entry);
			return;
		}

		prev = entry;
		entry = entry->next;
	}
}
