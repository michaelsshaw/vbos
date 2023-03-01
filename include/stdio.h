#ifndef _STDIO_H_
#define _STDIO_H_

#define printf_error(_fmt, ...) printf("[ERROR]: " _fmt, __VA_ARGS__);

int printf(const char *restrict fmt, ...);

#endif /* _SDTIO_H_ */
