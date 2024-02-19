/* SPDX-License-Identifier: GPL-2.0-only */
/* syscall handler for simple userland shell test */

#define __ASM__

.macro _sc_define name, num
.global \name
\name:
	movq $\num, %rax
	movq %rcx, %r10
	int $0x80
	ret
.endm

_sc_define read, 0
_sc_define write, 1
_sc_define open, 2
_sc_define close, 3
_sc_define exit, 60
