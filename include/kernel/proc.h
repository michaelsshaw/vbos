/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PROC_H_
#define _PROC_H_

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t pid_t;

struct procregs {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rip;

	uint64_t rflags;
	uint64_t rsp;
	uint16_t fs;
	uint16_t gs;
	uint16_t cs;
} PACKED;

struct proc {
	struct procregs regs;
	uint64_t pid;
	uint64_t ppid;
	uint64_t cr3;

	bool is_kernel;
};

struct procregs *proc_current_regs();
void proc_init(unsigned num_cpus);

#endif /* _PROC_H_ */
