/* SPDX-License-Identifier: GPL-2.0-only */

/* test1/start.S
 *
 * Test the basic functionality of the kernel loader
 */

sys_write = 1
sys_exit = 60

.global _start
_start:
	movl $sys_write, %eax
	movl $1, %edi
	movq $test_string, %rsi
	movl $test_string_len, %edx
	int $0x80

	movl $sys_exit, %eax
	movl $0xDEADC0DE, %edi
	int $0x80
_no_exit:
	movq $0, %rbx
	jmp _no_exit

.data
test_string:
	.ascii "hello, world\n\n"
test_string_len = . - test_string
