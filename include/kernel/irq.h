/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IRQ_H_
#define _IRQ_H_

#include <kernel/common.h>

int irq_map(uint8_t irq, void *func);
void irq_unmap(uint8_t irq);
int irq_highest_free();

#endif /* _IRQ_H_ */
