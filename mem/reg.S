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

#define genrww(reg)		\
.global reg##_read;		\
reg##_read:			\
	movw %##reg, %ax;	\
	ret;			\
.global reg##_write;		\
reg##_write:			\
	movw %di, %##reg;	\
	ret;

genrw(rsp)
genrw(rbp)
genrw(cr2)
genrw(cr3)
genrww(ds)

.global load_stack_and_park
load_stack_and_park:
	movq %rdi, %rsp
	movq %rsi, %rbp

__load_stack_and_park_no_escape:
	call yield
	jmp __load_stack_and_park_no_escape
