/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _TRAP_H_
#define _TRAP_H_

void exception(int vector, int error);
void exception_init();

#endif
