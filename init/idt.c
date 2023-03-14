/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <dev/serial.h>
#include <stdio.h>

#define exception(_i) _exception_##_i
#define exception_decl(_j) void exception(_j)()

exception_decl(0x00);
exception_decl(0x01);
exception_decl(0x02);
exception_decl(0x03);
exception_decl(0x04);
exception_decl(0x05);
exception_decl(0x06);
exception_decl(0x07);
exception_decl(0x08);
exception_decl(0x09);
exception_decl(0x0A);
exception_decl(0x0B);
exception_decl(0x0C);
exception_decl(0x0D);
exception_decl(0x0E);
exception_decl(0x0F);
exception_decl(0x10);
exception_decl(0x11);
exception_decl(0x12);
exception_decl(0x13);
exception_decl(0x14);
exception_decl(0x15);
exception_decl(0x16);
exception_decl(0x17);
exception_decl(0x18);
exception_decl(0x19);
exception_decl(0x1A);
exception_decl(0x1B);
exception_decl(0x1C);
exception_decl(0x1D);
exception_decl(0x1E);
exception_decl(0x1F);

void idt_load();
void isr_sched();

void sched_test()
{
	printf("a");
}

void idt_insert(uint16_t gate, uint8_t type, uint8_t ist, uint16_t segment, void *func)
{
	uintptr_t f = (uintptr_t)(func);
	struct irqdesc *irqdesc = (__idt + gate);
	irqdesc->offset_low = f & 0xFFFF;
	irqdesc->offset_mid = ((f >> 0x10) & 0xFFFF);
	irqdesc->offset_high = ((f >> 0x20) & 0xFFFFFFFF);

	irqdesc->segment = segment;
	irqdesc->ist = ist;
	irqdesc->attrs = type;
}

void idt_init()
{
	idt_insert(0x00, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x00));
	idt_insert(0x01, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x01));
	idt_insert(0x02, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x02));
	idt_insert(0x03, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x03));
	idt_insert(0x04, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x04));
	idt_insert(0x05, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x05));
	idt_insert(0x06, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x06));
	idt_insert(0x07, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x07));
	idt_insert(0x08, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x08));
	idt_insert(0x09, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x09));
	idt_insert(0x0A, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0A));
	idt_insert(0x0B, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0B));
	idt_insert(0x0C, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0C));
	idt_insert(0x0D, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0D));
	idt_insert(0x0E, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0E));
	idt_insert(0x0F, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x0F));
	idt_insert(0x10, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x10));
	idt_insert(0x11, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x11));
	idt_insert(0x12, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x12));
	idt_insert(0x13, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x13));
	idt_insert(0x14, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x14));
	idt_insert(0x15, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x15));
	idt_insert(0x16, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x16));
	idt_insert(0x17, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x17));
	idt_insert(0x18, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x18));
	idt_insert(0x19, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x19));
	idt_insert(0x1A, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1A));
	idt_insert(0x1B, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1B));
	idt_insert(0x1C, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1C));
	idt_insert(0x1D, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1D));
	idt_insert(0x1E, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1E));
	idt_insert(0x1F, GATE_TRAP, 0, GDT_SEGMENT_CODE_RING0, exception(0x1F));

	idt_insert(0x24, GATE_INTR, 0, GDT_SEGMENT_CODE_RING0, isr_serial_input);

	__idtr.size = (IDT_SIZE - 1);
	__idtr.offset = (uint64_t)__idt;
	idt_load();

	printf(LOG_SUCCESS "IDT successfully loaded\n");
}
