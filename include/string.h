/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STRING_H_
#define _STRING_H_

#ifndef __ASM__

#include <stdint.h>
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t num);
int memcmp(const void *aa, const void *bb, size_t num);
void *memset(void *str, int c, size_t n);
char *strcpy(char *dest, const char *src);
char *strtok(char *str, const char *delim);
char **strsplit(const char *str, char delim, int *num);
size_t strlen(const char *c);
int atoi(const char *a);

#endif /* __ASM__ */
#endif /* _STRING_H_ */
