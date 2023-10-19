/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STRING_H_
#define _STRING_H_

#ifndef __ASM__

#include <stdint.h>
#include <stddef.h>

#include <kernel/lock.h>

void *memcpy(void *dest, const void *src, size_t num);
int memcmp(const void *aa, const void *bb, size_t num);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *memset(void *str, int c, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strtok(char *str, const char *delim, char **llast);
char **strsplit(const char *str, char delim, int *num);
size_t strlen(const char *c);
int atoi(const char *a);

extern spinlock_t strtok_lock;

#endif /* __ASM__ */
#endif /* _STRING_H_ */
