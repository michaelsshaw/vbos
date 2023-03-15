/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/pio.h>
#include <dev/pic.h>

void pic_mask(uint8_t irq, uint8_t i)
{
	uint16_t port;
	uint8_t val;

	i = !(!i);

	if (irq < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		irq -= 8;
	}

	val = inb(port) & ~(1 << irq);
	val |= (i << irq);

	outb(port, val);
}

void pic_eoi(uint8_t irq)
{
	if (irq > 8)
		outb(PIC2_CMD, 0x20);

	outb(PIC1_CMD, 0x20);
}

void exception(int vector, int error)
{
	printf(LOG_WARN "EXCEPTION: %x, error code: %d\n", vector, error);
}

void pic_init()
{
	uint8_t mask1 = inb(PIC1_CMD + 1);
	uint8_t mask2 = inb(PIC2_CMD + 1);

	outb(PIC1_CMD, 0x11);
	iowait();
	outb(PIC2_CMD, 0x11);
	iowait();
	outb(PIC1_DATA, 0x20);
	iowait();
	outb(PIC2_DATA, 0x28);
	iowait();
	outb(PIC1_DATA, 4);
	iowait();
	outb(PIC2_DATA, 2);
	iowait();

	outb(PIC1_DATA, 0x01);
	iowait();
	outb(PIC2_DATA, 0x01);
	iowait();

	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);

	pic_eoi(15);
	printf(LOG_SUCCESS "8259 PIC initialized\n");
}
