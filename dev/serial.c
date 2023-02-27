#include <kernel/pio.h>

#define COM1 0x3F8

int serial_init()
{
	outb(COM1 | 1, 0x00);
	outb(COM1 | 3, 0x80);
	outb(COM1 | 0, 0x01);
	outb(COM1 | 1, 0x00);
	outb(COM1 | 3, 0x03);
	outb(COM1 | 2, 0xC7);
	outb(COM1 | 4, 0x0B);
	outb(COM1 | 4, 0x1E);

	outb(COM1 | 0, 0xAE); /* Enable loopback mode */
	if (inb(COM1 | 0) != 0xAE) { /* Loopback test */
		return 1;
	}

	outb(COM1 | 4, 0x0F); /* Enter normal operation */
	return 0;
}

void serial_write(char c)
{
	/* Wait for line to be clear */
	while ((inb(COM1 | 5) & 0x20) == 0)
		;

	outb(COM1, c);
}
