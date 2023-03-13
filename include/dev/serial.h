/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SERIAL_H_
#define _SERIAL_H_

#define COM1 0x3F8

void serial_write(char c);
int serial_init();

#endif /* _SERIAL_H_ */
