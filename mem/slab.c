/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>

#define SLAB_MIN_COUNT 64
#define SLAB_ALIGN 7

size_t slab_sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
struct slab *slab_cache[ARRAY_SIZE(slab_sizes)];

struct slab *kmap_slab = NULL;
struct kmap_entry *kmem_map = NULL;

static inline void slab_range(struct slab *slab, uintptr_t *o_start, uintptr_t *o_end)
{
	uintptr_t sslab = (uintptr_t)slab;
	sslab = (sslab | SLAB_ALIGN) + 1;

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

	uintptr_t aret = (uintptr_t)ret;
	uintptr_t start = (uintptr_t)ret + sizeof(struct slab);

	start |= SLAB_ALIGN;
	start += 1;

	ret->num = (((aret + asize) - start) / size);
	ret->size = size;
	ret->next = NULL;
	ret->prev = NULL;
	ret->nextfree = (uintptr_t *)start;
	ret->free = ret->num;

	for(size_t i = 0; i < ret->num - 1; i++) {
		*(uintptr_t *)(start + (i * size)) = start + ((i + 1) * size);
	}
	*(uintptr_t *)(start + ((ret->num - 1) * size)) = 0;


	return ret;
}

void *slab_alloc(struct slab *slab)
{
	if (slab == NULL)
		return NULL;

	if (slab->nextfree == NULL) {
		if (slab->next == NULL) {
			slab->next = slab_create(slab->size);
			slab->next->prev = slab;
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

		buddy_free(slab);
	}
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
