/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GDT_H_
#define _GDT_H_

#define GDT_NUMDESC 192
#define GDT_SIZE (64 * GDT_NUMDESC)

#define GDT_ACCESS_DATA_RING0 0b10010010
#define GDT_ACCESS_CODE_RING0 0b10011010

#define GDT_ACCESS_DATA_RING3 0b11110010
#define GDT_ACCESS_CODE_RING3 0b11111010

#define GDT_SEGMENT_CODE_RING0 0x28
#define GDT_SEGMENT_DATA_RING0 0x30

#define GDT_SEGMENT_DATA_RING3 0x38
#define GDT_SEGMENT_CODE_RING3 0x40

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

struct gdt_desc_sys {
	uint16_t limit;
	uint16_t base;
	uint8_t base_2;
	uint8_t access;
	uint8_t flags;
	uint8_t base_3;
	uint32_t base_4;
	uint32_t reserved;
} PACKED;

struct tss {
	uint32_t reserved;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved2;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved3;
	uint16_t reserved4;
	uint16_t iomap_base;
} PACKED;

struct gdtr {
	uint16_t size;
	uint64_t offset;
} PACKED;

extern struct gdtr __gdtr;
extern struct gdt_desc __gdt[GDT_NUMDESC];

void gdt_init();
uint16_t gdt_insert_tss(uintptr_t rsp0);
void ltr(uint16_t selector);

#endif /* __ASM__ */
#endif /* _GDT_H_ */
