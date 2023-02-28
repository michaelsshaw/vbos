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
