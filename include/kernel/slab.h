/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SLAB_H_
#define _SLAB_H_

#include <kernel/common.h>
#include <kernel/mem.h>

#define SLAB_PAGE_ALIGN 0x01
#define SLAB_DMA_64K 0x02

#define ALLOC_KERN 0x01
#define ALLOC_DMA 0x02

struct slab {
	size_t size; /* size of each object */
	size_t tsize; /* total size of this slab */
	size_t num; /* number of objects in this slab */
	size_t free; /* number of free objects */
	uint64_t flags;
	size_t align;
	uintptr_t *nextfree;
	struct slab *next;
	struct slab *prev;
};

struct kmap_entry {
	uintptr_t *ptr;
	size_t size;
	struct slab *slab;
	struct kmap_entry *next;
};

struct slab *slab_create(size_t size, size_t cache_size, uint64_t flags);
void *slab_alloc(struct slab *slab);
void slab_free(struct slab *slab, void *ptr);

void kmalloc_init();
void *kmalloc(size_t size, uint64_t flags);
void *kzalloc(size_t size, uint64_t flags);
void kfree(void *ptr);

#endif /* _SLAB_H_ */
