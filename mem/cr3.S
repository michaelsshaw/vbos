/* SPDX-License-Identifier: GPL-2.0-only */
.global cr3_get
cr3_get:
	movq %cr3, %rax
	ret
