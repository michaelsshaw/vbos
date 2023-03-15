/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <dev/serial.h>

#include <stdarg.h>

static inline void printf_out(char c)
{
	serial_write(c);
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

				char buf[11];
				n = 0;

				do {
					buf[n] = convert[i % 10];
					n++;
				} while ((i /= 10));

				for (int j = 0; j < n; j++) {
					printf_out(buf[(n - 1) - j]);
					ret++;
				}
				break;
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
				for (uint32_t j = 0; j < (2 * sizeof(i)); j++) {
					int sel = ((sizeof(i) * 2) - j - 1) * 4;
					int num = (i >> sel) & 0xF;
					printf_out(convert[num]);
					ret++;
				}
				break;
			case 'X':
				l = va_arg(args, long);
				for (uint64_t j = 0; j < (2 * sizeof(l)); j++) {
					int sel = ((sizeof(l) * 2) - j - 1) * 4;
					int num = (l >> sel) & 0xF;
					printf_out(convert[num]);
					ret++;
				}
				break;
			default:
				printf_out(c);
				ret++;
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
