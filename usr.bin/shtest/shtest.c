/* SPDX-License-Identifier: GPL-2.0-only */
/* shtest: simple userland shell test */
#include <stdint.h>

#define S_IFIFO 0x1000

typedef int64_t ssize_t;
typedef int64_t pid_t;

int open(const char *pathname, int flags);
ssize_t read(int fd, void *buf, int count);
ssize_t write(int fd, void *buf, int count);
int close(int fd);
pid_t fork();
int exit(int status);
int mknod(const char *pathname, int mode, int dev);

void promptfd(int fd)
{
	write(fd, "shtest> ", 8);
}

void _start()
{
	int term = open("/dev/tty0", 0);
	if (term < 0)
		exit(1);


	char buf[256];
	int n;

	/* FIFOs!!!!!!!!! */
	int r = mknod("/dev/pts", S_IFIFO, 0);
	int fifofd = open("/dev/pts", 0);
	if (fifofd < 0)
		write(term, "open failed\n", 12);

	/* write hello to fifo */
	int i = write(fifofd, "hello\n", 6);
	/* read back from fifo */
	i = read(fifofd, buf, 6);

	/* print output to terminal */
	write(term, buf, i);
	promptfd(term);

	/* fork test */
	pid_t pid = fork();
	if (pid < 0) {
		write(term, "fork failed\n", 12);
		exit(1);
	}

	if (pid == 0) {
		write(term, "child\n", 6);
		exit(1);
	} else {
		write(term, "parent\n", 7);
	}

	/* rw test */
	while ((n = read(term, buf, 256)) > 0) {
		write(term, buf, n);

		if(buf[0] == 'q')
			break;
	}

	exit(1234);
}
