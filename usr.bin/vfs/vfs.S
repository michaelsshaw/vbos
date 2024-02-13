/* SPDX-License-Identifier: GPL-2.0-only */
sys_write = 1
sys_open = 2
sys_exit = 60

.global _start
_start:
	movq $sys_open, %rax
	movq $tty_file, %rdi
	movq $0, %rsi
	movq $0, %rdx
	int $0x80

	movq %rax, %rbx

	movq $sys_write, %rax
	movq %rbx, %rdi
	leaq test_string(%rip), %rsi
	movq $test_string_len, %rdx
	int $0x80

	movq $sys_exit, %rax
	movq $0, %rdi
	int $0x80
_no_exit:
	movq $0, %rbx
	jmp _no_exit

.data
test_string:
	.ascii "hello, world\n\n"
test_string_len = . - test_string

tty_file:
	.asciz "/dev/tty"
