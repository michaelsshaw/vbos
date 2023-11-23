/* SPDX-License-Identifier: GPL-2.0-only */

/* test1/start.S
 *
 * Test the basic functionality of the kernel loader
 */

sys_exit = 60

.global _start
_start:
	movq test_var, %rdi
	movq $sys_exit, %rax
	int $0x80
_no_exit:
	movq $0, %rbx
	jmp _no_exit

.data
test_var:
	.long 0xfeedbeef
	.long 0xdeadbeef
