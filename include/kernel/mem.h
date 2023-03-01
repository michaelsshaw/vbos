/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MEM_H_
#define _MEM_H_

#include <kernel/common.h>

#ifndef __ASM__

#define HHDM_START 0xFFFF800000000000

#define ALIGN(_a) __attribute__((aligned(_a)))

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
			uint64_t pdpt_addr : 36;
			uint64_t reserved : 4;
			uint64_t ignored_3 : 11;
			uint64_t xd : 1;
		};
		uint64_t val;
	};
};

extern struct page pml4[512] ALIGN(0x1000);
extern struct page pdpt[512] ALIGN(0x1000);
extern struct page pdt[512] ALIGN(0x1000);
extern struct page pt[512] ALIGN(0x1000);

int page_map(uintptr_t virt, uintptr_t phys);
uint64_t cr3_get();

void pfa_init();

#endif /* __ASM__ */
#endif /* _MEM_H_ */
