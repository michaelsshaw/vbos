/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SLAB_H_
#define _SLAB_H_

#include <kernel/common.h>
#include <kernel/mem.h>
#include <kernel/lock.h>

#define SLAB_PAGE_ALIGN 0x01
#define SLAB_DMA_64K 0x02

#define ALLOC_KERN 0x01
#define ALLOC_DMA 0x02

#define ATTEMPT_FREE(_x)   \
	if (_x) {          \
		kfree(_x); \
		_x = NULL; \
	}

typedef struct _slab {
	size_t size; /* size of each object */
	size_t tsize; /* total size of this slab */
	size_t num; /* number of objects in this slab */
	size_t free; /* number of free objects */
	uint64_t flags;
	size_t align;
	uintptr_t *nextfree;
	struct _slab *next;
	struct _slab *prev;

	spinlock_t lock;
} slab_t;

struct kmap_entry {
	uintptr_t *ptr;
	size_t size;
	slab_t *slab;
	struct kmap_entry *next;
};

slab_t *slab_create(size_t size, size_t cache_size, uint64_t flags);
void *slab_alloc(slab_t *slab);
void slab_free(slab_t *slab, void *ptr);

void kmalloc_init();
void *kmalloc(size_t size, uint64_t flags);
void *kzalloc(size_t size, uint64_t flags);
void *krealloc(void *ptr, size_t size, uint64_t flags);
void kfree(void *ptr);

void slabtypes_init();

#endif /* _SLAB_H_ */
