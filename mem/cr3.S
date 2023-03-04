/* SPDX-License-Identifier: GPL-2.0-only */
.global cr3_read
cr3_read:
	movq %cr3, %rax
	ret

.global cr3_write
cr3_write:
	movq %rdi, %cr3
	ret
