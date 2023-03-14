/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PIO_H_
#define _PIO_H_

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	__asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void iowait()
{
	outb(0x80, 0);
}

#endif /* _PIO_H_ */
