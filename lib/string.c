/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>

void *memcpy(void *dest, const void *src, size_t num)
{
	char *d = (char *)dest;
	char *s = (char *)src;
	for (size_t i = 0; i < num; i++) {
		*d++ = *s++;
	}
	return dest;
}

char *strcpy(char *dest, const char *src)
{
	while ((*dest++ = *src++))
		;
	return dest;
}

size_t strlen(const char *c)
{
	const char *b = c;
	while (*b)
		b++;
	return b - c;
}

int memcmp(const void *aa, const void *bb, size_t num)
{
	const char *a = (const char *)aa;
	const char *b = (const char *)bb;
	for (size_t i = 0; i < num; i++) {
		if (*a++ != *b++)
			return a - b;
	}
	return 0;
}

void *memset(void *str, int c, size_t n)
{
	uintptr_t ptr = (uintptr_t)str;
	uintptr_t val = (uintptr_t)c;
	val |= val << 8;
	val |= val << 16;
	val |= val << 32;

	for (size_t i = 0; i < (n >> 3); i++) {
		*(uintptr_t *)ptr = val;
		ptr += 8;
	}

	if (n & 7) {
		ptr = (uintptr_t)str + (n & (~7ull));
		for (size_t i = 0; i < (n & 7); i++) {
			*(char *)ptr = c;
			ptr++;
		}
	}

	return str;
}

int atoi(const char *a)
{
	int ndigits = strlen(a);
	int b = 1;

	int ret = 0;

	for (int i = ndigits - 1; i >= 0; i--) {
		ret += b * (a[i] - '0');
		b *= 10;
	}
	return ret;
}
