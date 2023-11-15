/* SPDX-License-Identifier: GPL-2.0-only */

/* test1/start.S
 *
 * Test the basic functionality of the kernel loader
 */

 sys_exit = 60

.global _start
_start:
	call main
	movq %rax, %rdi
	movq sys_exit, %rax
	int $0x80
