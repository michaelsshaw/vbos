/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MEM_H_
#define _MEM_H_

#include <kernel/common.h>

#define HHDM_START 0xffffffff80000000

#ifndef __ASM__

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

extern uintptr_t __hhdm_start;
extern uintptr_t __data_end;
extern uintptr_t __bss_start;
extern uintptr_t __bss_end;

extern uintptr_t hhdm_start;
extern uintptr_t data_end;
extern uintptr_t bss_start;
extern uintptr_t bss_end;

int page_map(uintptr_t virt, uintptr_t phys);
uint64_t cr3_read();
void cr3_write(uint64_t cr3);

void pfa_init(char *mem, size_t len);

#endif /* __ASM__ */
#endif /* _MEM_H_ */
