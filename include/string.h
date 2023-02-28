#ifndef _STRING_H_
#define _STRING_H_

#ifndef __ASM__

#include <kernel/common.h>

void *memcpy(void *dest, const void *src, size_t num);
char *strcpy(char *dest, const char *src);

#endif /* __ASM__ */
#endif /* _STRING_H_ */
