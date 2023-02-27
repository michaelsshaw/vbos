#include <kernel/idt.h>

void idt_insert(uint16_t gate, uint8_t type, uint8_t ist,
		       uint16_t segment, void *func)
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
}
