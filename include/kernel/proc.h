/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PROC_H_
#define _PROC_H_

#include <stdint.h>
#include <stdbool.h>

#include <kernel/lock.h>
#include <kernel/rbtree.h>

#include <lib/sem.h>

#define PROC_RUNNING 0
#define PROC_STOPPED 1
#define PROC_BLOCKED 2

#define PT_USER 0
#define PT_KERN 1

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef int64_t pid_t;

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

typedef struct _kthread {
	struct proc *proc;
	uintptr_t stack;
	bool done;
	uint64_t retval;
} kthread_t;

struct proc {
	struct procregs regs;
	pid_t pid;
	pid_t ppid;
	uint64_t cr3;
	spinlock_t lock;

	bool is_kernel;
	uint8_t state;

	struct rbtree page_map;
	spinlock_t page_map_lock;

	struct rbtree fd_map;
	spinlock_t fd_map_lock;

	struct proc *parent;
	struct rbtree children;

	kthread_t *waiting_on;
};


struct procregs *proc_current_regs();
struct proc *proc_create();
struct proc *proc_fork(struct proc *parent);
struct proc *proc_get(pid_t pid);
pid_t getpid();
void proc_term(pid_t pid);
struct proc *proc_find(pid_t pid);
void proc_save_state();
void proc_set_current(pid_t pid);
void proc_init(unsigned num_cpus);
void trap_sched();
void schedule();
void syscall_block();

kthread_t *kthread_create();
void kthread_call(kthread_t *thread, uint64_t entry, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
uint64_t kthread_join(kthread_t *thread);
void kthread_yield();
kthread_t *kthread_current();
void sswtch();

#endif /* _PROC_H_ */
