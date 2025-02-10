/* SPDX-License-Identifier: GPL-2.0-only */
/* shtest: simple userland shell test */
#include <stdint.h>

typedef int64_t ssize_t;
typedef int64_t pid_t;

int open(const char *pathname, int flags);
ssize_t read(int fd, void *buf, int count);
ssize_t write(int fd, void *buf, int count);
int close(int fd);
pid_t fork();
int exit(int status);

void promptfd(int fd)
{
	write(fd, "shtest> ", 8);
}

void _start()
{
	int fd = open("/dev/tty0", 0);
	if (fd < 0)
		exit(1);

	promptfd(fd);

	char c[2];

	ssize_t ret;
	while ((ret = read(fd, c, 1)) > 0) {
		if (c[0] == '\r')
			c[0] = '\n';

		write(fd, c, 1);

		if (c[0] == '\n') {
			write(fd, "attempting to fork\n", 19);
			pid_t pid = fork();

			if (pid == 0) {
				write(fd, "child\n", 6);
				exit(0);
			} else {
				write(fd, "parent\n", 7);
			}
		}
	}

	exit(0x1234);
}
