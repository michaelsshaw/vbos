/* SPDX-License-Identifier: GPL-2.0-only */
.global rdmsr
rdmsr:
	pushq %rbp
	movq %rsp, %rbp

	movl %edi, %ecx
	rdmsr
	shlq $20, %rdx
	orq %rdx, %rax

	popq %rbp

	ret

.global wrmsr
wrmsr:
	pushq %rbp
	movq %rsp, %rbp

	movq %rsi, %rdx
	shrq $20, %rdx
	movl %esi, %eax
	movl %edi, %ecx
	wrmsr

	popq %rbp

	ret

