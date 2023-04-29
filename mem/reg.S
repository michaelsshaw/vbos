/* SPDX-License-Identifier: GPL-2.0-only */
.global cr3_read
cr3_read:
	movq %cr3, %rax
	ret

.global cr3_write
cr3_write:
	movq %rdi, %cr3
	ret

.global rsp_read
rsp_read:
	movq %rsp, %rax
	ret

.global rbp_read
rbp_read:
	movq %rbp, %rax
	ret

.global rsp_write
rsp_write:
	movq %rdi, %rsp
	ret

.global rbp_write
rbp_write:
	movq %rdi, %rbp
	ret

.global stack_init
stack_init:
	movq %rdi, %rsp
	movq %rsi, %rbp
__stack_init_no_escape:
	call kmain_post_stack_init
	jmp __stack_init_no_escape
