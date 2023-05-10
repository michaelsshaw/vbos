/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <dev/console.h>

#include <stdarg.h>

static inline void printf_out(char c)
{
	console_write(c);
}

static int printf_write(const char *restrict s)
{
	char c;
	int ret = 0;
	while ((c = *s++)) {
		printf_out(c);
		ret++;
	}
	return ret;
}

/* this is seriously the approach i'm going with for now */
int snprintf(char *str, char *restrict fmt, size_t size, ...)
{
	int ret = 0;
	char c;
	char *s;
	int i;
	long l;

	int n;

	va_list args;
	va_start(args, size);

	const char *convert = "0123456789ABCDEF";
	char buf[16] = { 0 };
	char *bufstr = NULL;

	while ((c = *fmt++) && (size_t)ret < size) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case '\0':
				str[ret] = '\0';
				ret++;
				goto out;
			case 'd':
				i = va_arg(args, int);

				if (i < 0) {
					str[ret] = '-';
					ret++;
					i = -i;
				}

				n = 0;
				memset(buf, 0, sizeof(buf));

				do {
					buf[n] = convert[i % 10];
					n++;
				} while ((i /= 10));

				goto printf_reverse;
			case 's':
				s = va_arg(args, char *);

				bufstr = s;
				while ((c = *bufstr++)) {
					if ((size_t)ret >= size)
						goto out;
					str[ret] = c;
					ret++;
				}
				break;
			case 'c':
				i = va_arg(args, int);
				ret++;
				str[ret] = i;
				break;
			case 'x':
				i = va_arg(args, int);
				n = 0;

				memset(buf, 0, sizeof(buf));

				do {
					buf[n] = convert[i & 0xF];
					n++;
				} while ((i >>= 4));

				goto printf_reverse;
				break;
			case 'X':
				l = va_arg(args, long);
				for (uint64_t j = 0; j < (sizeof(l) << 1); j++) {
					int sel = ((sizeof(l) << 1) - j - 1) << 2;
					int num = (l >> sel) & 0xF;
					if ((size_t)ret >= size)
						goto out;
					str[ret] = convert[num];
					ret++;
				}
				break;
			default:
				printf_out(c);
				ret++;
				break;
printf_reverse:
				for (int j = 0; j < n; j++) {
					if ((size_t)ret >= size)
						goto out;
					str[ret] = buf[(n - 1) - j];
					ret++;
				}
				break;
			}
		} else {
			str[ret] = c;
			ret++;
		}
	}
out:
	va_end(args);
	return ret;
}

int printf(const char *restrict fmt, ...)
{
	int ret = 0;
	char c;
	char *s;
	int i;
	long l;

	int n;

	va_list args;
	va_start(args, fmt);

	const char *convert = "0123456789ABCDEF";
	char buf[16] = { 0 };

	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case '\0':
				printf_out('%');
				ret++;
				goto out;
			case 'd':
				i = va_arg(args, int);

				if (i < 0) {
					printf_out('-');
					ret++;
					i = -i;
				}

				n = 0;
				memset(buf, 0, sizeof(buf));

				do {
					buf[n] = convert[i % 10];
					n++;
				} while ((i /= 10));

				goto printf_reverse;
			case 's':
				s = va_arg(args, char *);
				ret += printf_write(s);
				break;
			case 'c':
				i = va_arg(args, int);
				ret++;
				printf_out(i);
				break;
			case 'x':
				i = va_arg(args, int);
				n = 0;

				memset(buf, 0, sizeof(buf));

				do {
					buf[n] = convert[i & 0xF];
					n++;
				} while ((i >>= 4));

				goto printf_reverse;
				break;
			case 'X':
				l = va_arg(args, long);
				for (uint64_t j = 0; j < (sizeof(l) << 1); j++) {
					int sel = ((sizeof(l) << 1) - j - 1) << 2;
					int num = (l >> sel) & 0xF;
					printf_out(convert[num]);
					ret++;
				}
				break;
			default:
				printf_out(c);
				ret++;
				break;
printf_reverse:
				for (int j = 0; j < n; j++) {
					printf_out(buf[(n - 1) - j]);
					ret++;
				}
				break;
			}
		} else {
			printf_out(c);
			ret++;
		}
	}
out:
	va_end(args);
	return ret;
}
