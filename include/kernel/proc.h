/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PROC_H_
#define _PROC_H_

#include <stdint.h>
#include <stdbool.h>
#include <kernel/lock.h>
#include <kernel/rbtree.h>

#define PROC_RUNNING 0
#define PROC_STOPPED 1
#define PROC_BLOCKED 2

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

	/* IRETQ stack frame */
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;

} PACKED;

struct proc {
	struct procregs regs;
	uint64_t pid;
	uint64_t ppid;
	uint64_t cr3;

	bool is_kernel;
	uint8_t state;

	struct rbtree page_map;
	spinlock_t page_map_lock;
};

struct procregs *proc_current_regs();
struct proc *proc_create();
struct proc *proc_get(pid_t pid);
pid_t getpid();
void proc_term(pid_t pid);
struct proc *proc_find(pid_t pid);
void proc_set_current(pid_t pid);
void proc_init(unsigned num_cpus);
void schedule();

#endif /* _PROC_H_ */
