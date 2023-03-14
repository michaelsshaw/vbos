/* SPDX-License-Identifier: GPL-2.0-only */
#define __ASM__
#include <kernel/gdt.h>
#include <kernel/idt.h>

/*
 * Data section
 */
.data

.global __gdtr
__gdtr:
	.word 0
	.quad 0
.global __gdt
__gdt:
	.skip GDT_SIZE

.global __idtr
__idtr:
	.word 0
	.quad 0
.global __idt
__idt:
	.skip IDT_SIZE

/*
 * Code Section
 */
.text

.global gdt_load
gdt_load:
	lgdt __gdtr
	ret

.global idt_load
idt_load:
	lidt __idtr
	ret
