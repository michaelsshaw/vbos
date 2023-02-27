#ifndef _IDT_H_
#define _IDT_H_

#define GATE_INTR 0x8E
#define GATE_TRAP 0x8F

#define IDT_GATE_SIZE 16
#define IDT_NUM_VECTORS 256
#define IDT_SIZE 32768

#ifndef __ASM__

#include <stdint.h>

struct irqdesc {
	uint16_t offset_low;
	uint16_t segment;
	uint8_t ist;
	uint8_t attrs;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
};

struct idtr {
	uint16_t size;
	uint64_t offset;
};

extern struct idtr __idtr;
extern struct irqdesc __idt[IDT_NUM_VECTORS];

#endif /* __ASM__ */
#endif /* _IDT_H_ */
