/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PROC_H_
#define _PROC_H_

#include <stdint.h>
#include <stdbool.h>

#include <kernel/lock.h>
#include <kernel/rbtree.h>
#include <kernel/slab.h>

#include <lib/sem.h>

#define PROC_RUNNING 0
#define PROC_STOPPED 1
#define PROC_ALLOWSCHED 1
#define PROC_BLOCKED 2
#define PROC_YIELD 3
#define PROC_ZOMBIE 4

#define PT_USER 0
#define PT_KERN 1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define USER_STACK_BASE 0x0000070000000000
#define USER_HEAP_BASE  0x0000000100000000 /* 4GB */

#define PM_BUD 0
#define PM_SLB 1

typedef int64_t pid_t;

struct proc_mmap_entry {
	uintptr_t vaddr;
	paddr_t paddr;
	size_t len;
	uint64_t attr;
	uint64_t type;
};

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

struct proc_mapping {
	uintptr_t vaddr;
	paddr_t paddr;
	size_t len;
	uint64_t attr;
	uint64_t type;
	slab_t *slab;
	struct proc_mapping *next;
};

struct proc_allocation {
	struct proc_mapping *mappings;
	struct proc_mapping *last;
	size_t num_mappings;
};

struct proc {
	struct procregs regs;
	uint8_t state;
	pid_t pid;
	pid_t ppid;
	uint64_t cr3;
	spinlock_t lock;

	bool is_kernel;

	struct rbtree page_map;
	spinlock_t page_map_lock;

	struct rbtree fd_map;
	spinlock_t fd_map_lock;

	struct proc *parent;
	struct rbtree children;

	struct proc *buddy_proc;

	uintptr_t stack_start;
	uintptr_t stack_size;

	uintptr_t heap_start;

	struct rbtree umalloc_tree;
};

struct procregs *proc_current_regs();
struct proc *proc_create();
struct proc *proc_createv(int flags);
void proc_init_memory(struct proc *proc, uint64_t mem_flags);
void proc_init_page_tables(struct proc *proc);
struct proc *proc_fork(struct proc *parent);
struct proc *proc_get(pid_t pid);
pid_t getpid();
pid_t getupid();
void proc_term(pid_t pid);
struct proc *proc_find(pid_t pid);
void proc_save_state();
void proc_set_current(pid_t pid);
void proc_init(unsigned num_cpus);
void trap_sched();
void schedule();
void syscall_block();
void proc_set_exec_addr(struct proc *proc, uintptr_t addr);
void proc_set_flags(struct proc *proc, uint64_t flags);
void proc_set_stack(struct proc *proc, uintptr_t base, size_t size);
void proc_set_state(pid_t pid, uint8_t state);

void sswtch();

#endif /* _PROC_H_ */
