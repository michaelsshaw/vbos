/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _COMMON_H_
#define _COMMON_H_

#include <limine/limine.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include <kernel/bitwise.h>

#define PACKED __attribute__((packed))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define KB (1024ull)
#define MB (KB * KB)
#define GB (KB * MB)

#define RFLAGS_IF (1 << 9)

void yield();
void panic();

typedef uintptr_t paddr_t;
typedef uint32_t paddr32_t;
typedef int64_t ssize_t;

#endif /* _COMMON_H_ */
