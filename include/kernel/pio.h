/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PIO_H_
#define _PIO_H_

#include <stdint.h>

#define genio(suffix, type)                          \
	void out##suffix(uint16_t port, type value); \
	type in##suffix(uint16_t port);

genio(b, uint8_t);
genio(w, uint16_t);
genio(l, uint32_t);

#undef genio

void iowait();

#endif /* _PIO_H_ */
