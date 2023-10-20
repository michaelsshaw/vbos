/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>
#include <kernel/lock.h>

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

int strcmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return *a - *b;
		a++;
		b++;
	}
	return *a - *b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		if (a[i] != b[i])
			return a[i] - b[i];
		if (!a[i])
			break;
	}
	return 0;
}

char *strcpy(char *dest, const char *src)
{
	while ((*dest++ = *src++))
		;
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		dest[i] = src[i];
		if (!src[i])
			break;
	}
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

char *strchr(const char *s, int c)
{
	while (*s) {
		if (*s == c)
			return (char *)s;
		s++;
	}
	return NULL;
}

char *strtok(char *str, const char *delim, char **llast)
{
	if (str)
		(*llast) = str;

	if (!(*llast))
		return NULL;

	char *ret = (*llast);
	while (*(*llast)) {
		if (strchr(delim, *(*llast))) {
			*(*llast) = 0;
			(*llast)++;
			return ret;
		}
		(*llast)++;
	}

	if(!(*ret))
		return NULL;

	return ret;
}

char **strsplit(const char *s, char delim, int *n)
{
	int num = 0;
	for (int i = 0; s[i]; i++) {
		if (s[i] == delim)
			num++;
	}

	char **ret = kmalloc((num + 2) * sizeof(char *), ALLOC_KERN);
	int idx = 0;
	int last = 0;
	for (int i = 0; s[i]; i++) {
		if (s[i] == delim) {
			ret[idx] = kmalloc(i - last + 1, ALLOC_KERN);
			memcpy(ret[idx], s + last, i - last);
			ret[idx][i - last] = 0;
			idx++;
			last = i + 1;
		}
	}
	ret[idx] = kmalloc(strlen(s) - last + 1, ALLOC_KERN);
	memcpy(ret[idx], s + last, strlen(s) - last);
	ret[idx][strlen(s) - last] = 0;
	idx++;
	ret[idx] = NULL;
	if (n)
		*n = idx;
	return ret;
}
