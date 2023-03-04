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
	unsigned char *s = (unsigned char *)str;
	unsigned char j = (unsigned char)c;
	for (size_t i = 0; i < n; i++) {
		s[i] = j;
	}
	return str;
}
