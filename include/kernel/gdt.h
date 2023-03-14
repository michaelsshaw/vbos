/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GDT_H_
#define _GDT_H_

#define GDT_NUMDESC 64
#define GDT_SIZE (64 * GDT_NUMDESC)

#define GDT_ACCESS_DATA_RING0 0b10010010
#define GDT_ACCESS_CODE_RING0 0b10011010

#define GDT_ACCESS_DATA_RING3 0b11110010
#define GDT_ACCESS_CODE_RING3 0b11111010

#define GDT_SEGMENT_CODE_RING0 0x28

#ifndef __ASM__

#include <kernel/common.h>

struct gdt_desc {
	uint16_t limit;
	uint16_t base;
	uint8_t base_2;
	uint8_t access;
	uint8_t flags;
	uint8_t base_3;
} PACKED;

struct gdtr {
	uint16_t size;
	uint64_t offset;
} PACKED;

extern struct gdtr __gdtr;
extern struct gdt_desc __gdt[GDT_NUMDESC];

void gdt_init();

#endif /* __ASM__ */
#endif /* _GDT_H_ */
