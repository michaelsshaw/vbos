/* SPDX-License-Identifier: GPL-2.0-only */

#define __ASM__
#include <dev/serial.h>
#include <lib/errno.h>

.macro save_context
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11

	/* get current task registers */
	call proc_current_regs

	/* save current task registers */
	popq %rdi
	movq %rdi, 80(%rax) /* r11 */
	popq %rdi
	movq %rdi, 72(%rax) /* r10 */
	popq %rdi
	movq %rdi, 64(%rax) /* r9 */
	popq %rdi
	movq %rdi, 56(%rax) /* r8 */
	popq %rdi
	movq %rdi, 40(%rax) /* rdi */
	popq %rdi
	movq %rdi, 32(%rax) /* rsi */
	popq %rdi
	movq %rdi, 24(%rax) /* rdx */
	popq %rdi
	movq %rdi, 16(%rax) /* rcx */
	popq %rdi
	movq %rdi, 0(%rax) /* rax */

	/* save remaining registers */
	movq %rbx, 8(%rax) 
	movq %rbp, 48(%rax)
	movq %r12, 88(%rax)
	movq %r13, 96(%rax)
	movq %r14, 104(%rax)
	movq %r15, 112(%rax)
.endm

.macro save_iretq_frame
	/* save IRETQ frame */
	popq %rdi
	movq %rdi, 120(%rax) 
	popq %rdi
	movq %rdi, 128(%rax)
	popq %rdi
	/* enable interrupts */
	movq %rdi, 136(%rax)
	popq %rdi
	movq %rdi, 144(%rax)
	popq %rdi
	movq %rdi, 152(%rax)
.endm

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
		save_context
		save_iretq_frame
		/* call irq handler */
		movq $\nr, %rdi
		call irq

		call schedule
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

.global gate_syscall
gate_syscall:
	pushq %r11
	pushq %rcx
	movq %r10, %rcx
	movq %rax, %r9

	call syscall

	popq %rcx
	popq %r11
	sysretq

.global gate_syscall_int80
gate_syscall_int80:
	cli
	/* stack frame */
	pushq %rbp
	movq %rsp, %rbp

	subq $88, %rsp

	/* save registers */
	movq %rdi, -8(%rbp)
	movq %rsi, -16(%rbp)
	movq %rdx, -24(%rbp)
	movq %rcx, -32(%rbp)
	movq %r8, -40(%rbp)
	movq %r9, -48(%rbp)
	movq %r10, -56(%rbp)
	movq %r11, -64(%rbp)
	movq %rax, -72(%rbp)

	/* save context */
	call proc_current_regs
	movq -72(%rbp), %rdi /* rax */
	movq %rdi, 0(%rax)
	movq %rbx, 8(%rax) /* rbx */
	movq -32(%rbp), %rdi /* rcx */
	movq %rdi, 16(%rax)
	movq -24(%rbp), %rdi /* rdx */
	movq %rdi, 24(%rax)
	movq -16(%rbp), %rdi /* rsi */
	movq %rdi, 32(%rax)
	movq -8(%rbp), %rdi /* rdi */
	movq %rdi, 40(%rax)
	movq (%rbp), %rdi
	movq %rdi, 48(%rax) /* rbp */
	movq -40(%rbp), %rdi /* r8 */
	movq %rdi, 56(%rax)
	movq -48(%rbp), %rdi /* r9 */
	movq %rdi, 64(%rax)
	movq -56(%rbp), %rdi /* r10 */
	movq %rdi, 72(%rax)
	movq -64(%rbp), %rdi /* r11 */
	movq %rdi, 80(%rax)
	movq %r12, 88(%rax)
	movq %r13, 96(%rax)
	movq %r14, 104(%rax)
	movq %r15, 112(%rax)


	/* save IRETQ frame */
	movq 8(%rbp), %rdi
	movq %rdi, 120(%rax) /* rip */
	movq 16(%rbp), %rdi
	movq %rdi, 128(%rax) /* cs */
	movq 24(%rbp), %rdi
	movq %rdi, 136(%rax) /* rflags */
	movq 32(%rbp), %rdi
	movq %rdi, 144(%rax) /* rsp */
	movq 40(%rbp), %rdi
	movq %rdi, 152(%rax) /* ss */

	/* restore argument registers */
	movq 40(%rax), %rdi
	movq 32(%rax), %rsi
	movq 24(%rax), %rdx
	movq 72(%rax), %r10
	movq 56(%rax), %r8
	movq 0(%rax), %rax

	/* leave stack frame */
	addq $88, %rsp
	popq %rbp



	movq %r10, %rcx
	movq %rax, %r9

	call syscall
	iretq

.global sswtch
sswtch:
	save_context
	movw %ss, %di

	movq %rdi, 152(%rax)
	pushfq /* save flags */
	popq %rdi
	movq %rsp, 144(%rax)
	movq %rdi, 136(%rax)
	movw %cs, %di
	movq %rdi, 128(%rax)
	movq $_sswtch_ret, 120(%rax)

	call schedule
_sswtch_ret:
	ret

/* void return_from_irq(struct procregs *regs, paddr_t cr3); */
.global _return_to_user
_return_to_user:
	/* restore IRETQ frame */
	pushq 152(%rdi)
	pushq 144(%rdi)
	pushq 136(%rdi)
	pushq 128(%rdi)
	pushq 120(%rdi)

	/* restore task registers */
	movq 0(%rdi), %rax
	movq 8(%rdi), %rbx
	movq 16(%rdi), %rcx
	movq 24(%rdi), %rdx
	movq 48(%rdi), %rbp
	movq 56(%rdi), %r8
	movq 64(%rdi), %r9
	movq 72(%rdi), %r10
	movq 80(%rdi), %r11
	movq 88(%rdi), %r12
	movq 96(%rdi), %r13
	movq 104(%rdi), %r14
	movq 112(%rdi), %r15

	/* load new page tables */
	movq %rsi, %cr3

	/* restore argument registers */
	movq 32(%rdi), %rsi
	movq 40(%rdi), %rdi
	
	iretq

.global yield
yield:
	hlt
	jmp yield
