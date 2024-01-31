/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STDIO_H_
#define _STDIO_H_

#define COL_RED_BLACK "\e[31m"
#define COL_YELLOW_BLACK "\e[33m"
#define COL_GREEN_BLACK "\e[32m"
#define COL_MAGENTA_BLACK "\e[35m"

#define COL_RESET "\e[m"

#define LOG_ERROR "[" COL_RED_BLACK "ERR" COL_RESET "] "
#define LOG_WARN "[" COL_YELLOW_BLACK "WRN" COL_RESET "] "
#define LOG_DEBUG "[" COL_MAGENTA_BLACK "DBG" COL_RESET "] "
#define LOG_SUCCESS "[" COL_GREEN_BLACK "RDY" COL_RESET "] "
#define LOG_INFO "[LOG] "

#include <stddef.h>
#include <stdarg.h>

int kprintf(const char *restrict fmt, ...);
int snprintf(char *str, const char *restrict fmt, size_t size, ...);
int vsnprintf(char *str, const char *restrict fmt, size_t size, va_list args);

#endif /* _SDTIO_H_ */
