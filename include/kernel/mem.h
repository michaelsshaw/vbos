/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MEM_H_
#define _MEM_H_

#include <kernel/common.h>

#define BBMAP_FREE 0
#define BBMAP_SPLIT 1
#define BBMAP_USED 2

#define PAGE_XD (1 << 64)
#define PAGE_PRESENT (1 << 0)
#define PAGE_RW (1 << 1)
#define PAGE_PCD (1 << 4)
#define PAGE_GLOBAL (1 << 8)
#define PAGE_USERMODE (1 << 2)

#ifndef __ASM__

#define ALIGN(_a) __attribute__((aligned(_a)))

#define genrw(reg)             \
	uint64_t reg##_read(); \
	void reg##_write(uint64_t reg);

genrw(rsp);
genrw(rbp);
genrw(cr3);

#undef genrw

struct page { /* I would rather refer to this as a struct */
	union {
		struct {
			uint64_t present : 1;
			uint64_t rw : 1;
			uint64_t usermode : 1;
			uint64_t pwt : 1;
			uint64_t pcd : 1;
			uint64_t accessed : 1;
			uint64_t ignored : 1;
			uint64_t zero : 1;
			uint64_t ignored_2 : 4;
			uint64_t addr : 36;
			uint64_t reserved : 4;
			uint64_t ignored_3 : 11;
			uint64_t xd : 1;
		};
		uint64_t val;
	};
};

struct cr3 {
	union {
		struct {
			uint64_t ignored : 3;
			uint64_t pwt : 1;
			uint64_t pcd : 1;
			uint64_t ignored_2 : 7;
			uint64_t pml4_addr : 52;
		};
		uint64_t val;
	};
};

struct mem_region {
	paddr_t base;
	size_t len;
	bool usable;
};

struct buddy_region_header {
	paddr_t usable_base;
	size_t usable_len;
	size_t max_depth;

	size_t bitmap_len;
	char bitmap[];
};

extern uintptr_t __data_end;
extern uintptr_t __bss_start;
extern uintptr_t __bss_end;

extern uintptr_t hhdm_start;
extern uintptr_t data_end;
extern uintptr_t bss_start;
extern uintptr_t bss_end;

extern struct mem_region *regions;
extern size_t num_regions;
extern size_t max_regions;

extern struct page *pml4;
extern paddr_t kcr3;
extern struct page kdefault_attrs;

void *buddy_alloc(size_t size);
void buddy_free(void *paddr_hhdm);
void kmap(paddr_t paddr, uint64_t vaddr, size_t len, uint64_t attr);

void mem_early_init(char *mem, size_t len);

#endif /* __ASM__ */
#endif /* _MEM_H_ */
