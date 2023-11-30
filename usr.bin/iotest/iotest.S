/* SPDX-License-Identifier: GPL-2.0-only */

/* ftest/ftest.S
 *
 * Test the read and write system calls.
 */

sys_read = 0
sys_write = 1
sys_exit = 60

STDIN_FILENO = 0
STDOUT_FILENO = 1

.macro print text text_size
	movl $sys_write, %eax
	movl $STDOUT_FILENO, %edi
	movq $\text, %rsi
	movl $\text_size, %edx
	int $0x80
.endm

.text
.global _start
_start:
_exit:
	print entry_message entry_message_size
	
	movl $sys_read, %eax
	movl $STDIN_FILENO, %edi
	movq $buf, %rsi
	movl $buf_size, %edx
	int $0x80

	movq %rax, %rdx
	movl $sys_write, %eax
	movl $STDOUT_FILENO, %edi
	movq $buf, %rsi
	int $0x80

	movq %rax, %rdx
	movl $sys_write, %eax
	movl $STDOUT_FILENO, %edi
	movq $buf, %rsi
	int $0x80

	movl $sys_exit, %eax
	movl $0, %edi
	int $0x80
.data
entry_message:
	.ascii "Please enter your text: "
entry_message_size = . - entry_message

exit_message:
	.ascii "Exiting successfully\n"
exit_message_size = . - exit_message

.bss
buf:
	.skip 1024
buf_size = . - buf
