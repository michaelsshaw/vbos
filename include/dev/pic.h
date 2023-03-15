/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PIC_H_
#define _PIC_H_

#include <kernel/common.h>

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#ifndef __ASM__

void pic_mask(uint8_t irq, uint8_t i);
void pic_eoi(uint8_t irq);
void pic_init();

#endif /* __ASM__ */
#endif /* _PIC_H_ */
