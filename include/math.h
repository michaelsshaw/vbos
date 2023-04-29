/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MATH_H_
#define _MATH_H_

#include <stdint.h>
#include <stddef.h>

size_t log2(size_t n);
size_t npow2(size_t n);
void bitmap_set(char *bitmap, size_t n, uint64_t b);
void bitmap_clear(char *bitmap, size_t n);
uint8_t bitmap_get(char *bitmap, size_t n);

#define MIN(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b; \
})

#define MAX(a, b) ({ \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b; \
})

#endif /* _MATH_H_ */
