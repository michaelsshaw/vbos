/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _COMMON_H_
#define _COMMON_H_

#include <limine/limine.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <kernel/bitwise.h>

#define PACKED __attribute__((packed))

typedef uintptr_t paddr_t;

#endif /* _COMMON_H_ */
