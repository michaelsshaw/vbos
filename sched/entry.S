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

.macro restore_context
	call proc_current_regs
	movq 8(%rax), %rbx
	movq 16(%rax), %rcx
	movq 24(%rax), %rdx
	movq 32(%rax), %rsi
	movq 40(%rax), %rdi
	movq 48(%rax), %rbp
	movq 56(%rax), %r8
	movq 64(%rax), %r9
	movq 72(%rax), %r10
	movq 80(%rax), %r11
	movq 88(%rax), %r12
	movq 96(%rax), %r13
	movq 104(%rax), %r14
	movq 112(%rax), %r15
	movq 0(%rax), %rax
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

.macro restore_iretq_frame
	/* restore IRETQ frame */
	pushq %rax
	call proc_current_regs
	popq %rdi
	pushq 152(%rax)
	pushq 144(%rax)
	pushq 136(%rax)
	pushq 128(%rax)
	pushq 120(%rax)
	movq %rdi, %rax
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
		save_context
		save_iretq_frame
		movq $\nr, %rdi                                         
		movq $0, %rsi                                           
		call exception                                          
		restore_iretq_frame
		restore_context
		iretq
.endm

.macro exception_err nr
	.global _exception_\nr 					
	_exception_\nr: 						
		cli
		save_context
		popq %rsi 						
		pushq %rsi
		movq $\nr, %rdi 					
		call exception 						
		restore_iretq_frame
		restore_context
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
		restore_iretq_frame
		restore_context
		iretq
.endm

.global _save_context
_save_context:
	save_context
	ret

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
	save_context
	save_iretq_frame

	/* restore argument registers */
	movq 40(%rax), %rdi
	movq 32(%rax), %rsi
	movq 24(%rax), %rdx
	movq 72(%rax), %r10
	movq 56(%rax), %r8
	movq 0(%rax), %rax

	movq %r10, %rcx
	movq %rax, %r9

	call syscall

	restore_iretq_frame
	pushq %rax
	restore_context
	popq %rax

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

.global _kthread_exit
_kthread_exit:
	movq %rax, %rdi
	call kthread_exit
