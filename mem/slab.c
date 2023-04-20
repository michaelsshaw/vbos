/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/slab.h>

#define SLAB_MIN_COUNT 16

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

	if ((uintptr_t)ptr < low || (uintptr_t)ptr > high)
		slab_free(slab->next, ptr);

	*(uintptr_t *)ptr = (uintptr_t)slab->nextfree;
	slab->nextfree = ptr;
}
