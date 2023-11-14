/* SPDX-License-Identifier: GPL-2.0-only */

#define __ASM__
#include <dev/serial.h>

.macro push_regs
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
.endm

.macro pop_regs
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rbp
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
.endm

.macro exception nr
	.global _exception_\nr 					
	_exception_\nr: 						
		cli
		push_regs
		movq $\nr, %rdi                                         
		movq $0, %rsi                                           
		call exception                                          
		call yield
		pop_regs
		iretq
.endm

.macro exception_err nr
	.global _exception_\nr 					
	_exception_\nr: 						
		cli
		push_regs
		popq %rsi 						
		movq $\nr, %rdi 					
		call exception 						
		call yield
		pop_regs
		iretq
.endm

.macro irq nr
	.global _irq_\nr 					
	_irq_\nr: 						
		cli
		push_regs
		movq $\nr, %rdi 					
		call irq 						
		pop_regs
		iretq
.endm

.global sti
sti:
	sti
	ret

.global cli
cli:
	cli
	ret

exception 0x00
exception 0x01
exception 0x02
exception 0x03
exception 0x04
exception 0x05
exception 0x06
exception 0x07
exception_err 0x08
exception 0x09
exception_err 0x0A
exception_err 0x0B
exception_err 0x0C
exception_err 0x0D
exception_err 0x0E
exception 0x0F
exception 0x10
exception_err 0x11
exception 0x12
exception 0x13
exception 0x14
exception_err 0x15
exception 0x16
exception 0x17
exception 0x18
exception 0x19
exception 0x1A
exception 0x1B
exception 0x1C
exception_err 0x1D
exception_err 0x1E
exception 0x1F

irq 0x00
irq 0x01
irq 0x02
irq 0x03
irq 0x04
irq 0x05
irq 0x06
irq 0x07
irq 0x08
irq 0x09
irq 0x0A
irq 0x0B
irq 0x0C
irq 0x0D
irq 0x0E
irq 0x0F

.global _irq_schedule
_irq_schedule:
	cli
	push_regs
	movq $0x00, %rdi
	call irq_schedule
	pop_regs
	iretq
