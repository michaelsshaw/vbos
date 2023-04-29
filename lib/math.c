/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <math.h>

size_t npow2(size_t n)
{
	if (n == 0)
		return 1;

	n--;
	n |= (n >> 1);
	n |= (n >> 2);
	n |= (n >> 4);
	n |= (n >> 8);
	n |= (n >> 16);
	n |= (n >> 32);
	return n + 1;
}

size_t log2(size_t n)
{
	return sizeof(n) * 8 - __builtin_clzll(n) - 1;
}

inline void bitmap_set(char *bitmap, size_t n, uint64_t b)
{
	if(b)
		bitmap[n >> 3] |= (1 << (n & 7));
	else
		bitmap[n >> 3] &= (~(1 << (n & 7)));
}

inline uint8_t bitmap_get(char *bitmap, size_t n)
{
	return bitmap[n >> 3] & (1 << (n & 7));
}
