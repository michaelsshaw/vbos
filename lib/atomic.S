/* SPDX-License-Identifier: GPL-2.0-only */
#define __ASM__

/* int atomic_cmpxchg(volatile int *ptr, int cmpval, int newval) */
.global atomic_cmpxchg
atomic_cmpxchg:
	/* if (*ptr == cmpval) *ptr = newval, but atomic :) */
	movl %esi, %eax
	lock cmpxchg %edx, (%rdi)
	
	/* if ZF is set, the exchange was successful */
	jnz nz
	xorl %eax, %eax
	ret
nz:
	movl $1, %eax
	ret
