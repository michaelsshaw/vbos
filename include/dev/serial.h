/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SERIAL_H_
#define _SERIAL_H_

#define COM1 0x3F8

#ifndef __ASM__

void isr_serial_input();
void isr_serial_input_1();
void serial_write(char c);
void serial_trap();
int serial_init();

#endif

#endif /* _SERIAL_H_ */
