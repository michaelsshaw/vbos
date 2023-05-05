/* SPDX-License-Identifier: GPL-2.0-only */
#define __ASM__
#include <dev/serial.h>

.macro exception nr
	.global _exception_\nr 					
	_exception_\nr: 						
		movq $\nr, %rdi                                         
		movq $0, %rsi                                           
		call exception                                          
		call yield
		iretq
.endm

.macro exception_err nr
	.global _exception_\nr 					
	_exception_\nr: 						
		popq %rsi 						
		movq $\nr, %rdi 					
		call exception 						
		call yield
		iretq
.endm

.macro irq nr
	.global _irq_\nr 					
	_irq_\nr: 						
		cli
		movq $\nr, %rdi 					
		call irq 						
		sti
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

