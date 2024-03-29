/* SPDX-License-Identifier: GPL-2.0-only */
.global rdmsr
rdmsr:
	movl %edi, %ecx
	rdmsr
	shlq $0x20, %rdx
	orq %rdx, %rax

	ret

.global wrmsr
wrmsr:
	movq %rsi, %rdx
	shrq $0x20, %rdx
	movl %esi, %eax
	movl %edi, %ecx
	wrmsr

	ret
