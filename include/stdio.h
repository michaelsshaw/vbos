/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _STDIO_H_
#define _STDIO_H_

#define COL_RED_BLACK "\e[31;40m"
#define COL_YELLOW_BLACK "\e[33;40m"
#define COL_GREEN_BLACK "\e[32;40m"
#define COL_MAGENTA_BLACK "\e[35;40m"

#define COL_RESET "\e[m"

#define LOG_ERROR "[" COL_RED_BLACK "  ERROR  " COL_RESET "] "
#define LOG_WARN "[" COL_YELLOW_BLACK " WARNING " COL_RESET "] "
#define LOG_SUCCESS "[" COL_GREEN_BLACK " SUCCESS " COL_RESET "] "
#define LOG_INFO "[  INFO   ] "

int printf(const char *restrict fmt, ...);

#endif /* _SDTIO_H_ */
