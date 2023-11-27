/* SPDX-License-Identifier: GPL-2.0-only */

/* ftest/ftest.S
 *
 * Test the read and write system calls.
 */

sys_read = 0
sys_write = 1
sys_open = 2
sys_exit = 60

.macro print text text_size
	movl $sys_write, %eax
	movl $1, %edi
	movq $\text, %rsi
	movl $\text_size, %edx
	int $0x80
.endm

.text
.global _start
_start:
	/* Open the file */
	movl $sys_open, %eax
	movq $filename, %rdi
	int $0x80

	cmpl $-1, %eax
	je _error_open

	/* Read the file */
	movq %rax, %rdi
	movl $sys_read, %eax
	movq $buf, %rsi
	movl $buf_size, %edx
	int $0x80

	cmpl $-1, %eax
	je _error_read

	/* Write the file */
	movl $sys_write, %eax
	movl $1, %edi
	movq $buf, %rsi
	movl $buf_size, %edx
	int $0x80

	cmpl $-1, %eax
	je _error_write

	print exit_message exit_message_size
	movl $0, %edi
	jmp _exit

_error_open:
	pushq %rax
	print error_open error_open_size
	popq %rdi
	jmp _exit

_error_read:
	pushq %rax
	print error_read error_read_size
	popq %rdi
	jmp _exit

_error_write:
	pushq %rax
	print error_write error_write_size
	popq %rdi
	jmp _exit

_exit:
	
	/* Exit */
	movl $sys_exit, %eax
	int $0x80

.data
filename:
	.asciz "test.txt"

error_open:
	.asciz "Error opening file\n"
error_open_size = . - error_open

error_read:
	.asciz "Error reading file\n"
error_read_size = . - error_read

error_write:
	.asciz "Error writing file\n"
error_write_size = . - error_write

exit_message:
	.asciz "Exiting successfully\n"
exit_message_size = . - exit_message

.bss
buf:
	.skip 1024
buf_size = . - buf
