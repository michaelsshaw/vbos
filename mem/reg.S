/* SPDX-License-Identifier: GPL-2.0-only */

#define genrw(reg)		\
.global reg##_read;		\
reg##_read:			\
	movq %##reg, %rax;	\
	ret;			\
.global reg##_write;		\
reg##_write:			\
	movq %rdi, %##reg;	\
	ret;


genrw(rsp)
genrw(rbp)
genrw(cr3)

.global stack_init
stack_init:
	movq %rdi, %rsp
	movq %rsi, %rbp
	call kmain_post_stack_init
__stack_init_no_escape:
	call yield
	jmp __stack_init_no_escape
