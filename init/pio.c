/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/pio.h>

#define genio(suffix, type)                                                           \
	inline void out##suffix(uint16_t port, type value)                            \
	{                                                                             \
		__asm__ volatile("out" #suffix " %0, %1" : : "a"(value), "Nd"(port)); \
	}                                                                             \
	inline type in##suffix(uint16_t port)                                         \
	{                                                                             \
		type ret;                                                             \
		__asm__ volatile("in" #suffix " %1, %0" : "=a"(ret) : "Nd"(port));    \
		return ret;                                                           \
	}

inline void iowait()
{
	outb(0x80, 0);
}

genio(b, uint8_t);
genio(w, uint16_t);
genio(l, uint32_t);
