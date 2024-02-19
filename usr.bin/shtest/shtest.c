/* SPDX-License-Identifier: GPL-2.0-only */
/* shtest: simple userland shell test */
#include <stdint.h>
#include <stddef.h>

typedef int64_t ssize_t;

int open(const char *pathname, int flags);
ssize_t read(int fd, void *buf, int count);
ssize_t write(int fd, void *buf, int count);
int close(int fd);
int exit(int status);

void promptfd(int fd)
{
	write(fd, "shtest> ", 8);
}

void _start()
{
	int fd = open("/dev/tty", 0);
	if (fd < 0)
		exit(1);

	promptfd(fd);

	char c;
	while (read(fd, &c, 1) == 1) {
		write(fd, &c, 1);

		if (c == '\n') {
			promptfd(fd);
		}
	}

	close(fd);
	exit(0);
}
