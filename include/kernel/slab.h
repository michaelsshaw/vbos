/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SLAB_H_
#define _SLAB_H_

#include <kernel/common.h>
#include <kernel/mem.h>

struct slab {
	size_t size; /* size of each object */
	size_t num; /* number of objects in this slab */
	size_t free; /* number of free objects */
	uintptr_t *nextfree;
	struct slab *next;
};

struct slab *slab_create(size_t size);
void *slab_alloc(struct slab *slab);
void slab_free(struct slab *slab, void *ptr);

#endif /* _SLAB_H_ */
