/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _COMMON_H_
#define _COMMON_H_

#define KSTACK_SIZE 0x4000

#include <limine/limine.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/errno.h>
#include <lib/math.h>
#include <lib/assert.h>

#include <kernel/bitwise.h>

#define PACKED __attribute__((packed))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define ATTEMPT_WRITE(_x, _y) \
	if (_x) {             \
		*(_x) = _y;      \
	}

#define KB (1024ull)
#define MB (KB * KB)
#define GB (KB * MB)

#define RFLAGS_IF (1 << 9)

void yield();
void panic();
void cli();
void sti();
void _save_context();
void reboot();
void load_stack_and_jump(uintptr_t rsp, uintptr_t rbp, void *func, void *arg);

typedef uintptr_t paddr_t;
typedef uint32_t paddr32_t;
typedef int64_t ssize_t;

#endif /* _COMMON_H_ */
