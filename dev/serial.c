/* SPDX-License-Identifier: GPL-2.0-only */
#include "fs/devfs.h"
#include "fs/vfs.h"
#include <kernel/common.h>
#include <kernel/pio.h>
#include <kernel/proc.h>
#include <kernel/umem.h>
#include <kernel/syscall.h>
#include <kernel/char.h>
#include <kernel/ringbuf.h>

#include <dev/serial.h>
#include <dev/apic.h>

struct char_device *tty0;
ssize_t serial_write_dev(void *dev, void *buf, size_t offset, size_t count);
ssize_t serial_read_dev(void *dev, void *buf, size_t offset, size_t count);

struct devfs_dev_info serial_dev_ops = {
	.read = serial_read_dev,
	.write = serial_write_dev,
};

int serial_init()
{
	outb(COM1 | 1, 0x00);
	outb(COM1 | 3, 0x80);
	outb(COM1 | 0, 0x01);
	outb(COM1 | 1, 0x00);
	outb(COM1 | 3, 0x03);
	outb(COM1 | 2, 0xC7);
	outb(COM1 | 4, 0x0B);
	outb(COM1 | 4, 0x1E);

	outb(COM1 | 0, 0xAE); /* Enable loopback mode */
	if (inb(COM1 | 0) != 0xAE) { /* Loopback test */
		kprintf(LOG_WARN "Serial COM1 loopback test failed\n");
		return 1;
	}

	outb(COM1 | 1, 0x01); /* Enable interrupts */
	outb(COM1 | 4, 0x0F); /* Enter normal operation */
	kprintf(LOG_SUCCESS "Serial COM1 initialized\n");

	tty0 = char_device_create("tty0", ringbuf_create(1024));
	serial_dev_ops.dev = tty0;

	devfs_insert(NULL, "tty0", VFS_VNO_CHARDEV, &serial_dev_ops);

	/* register char device */
	return 0;
}

inline void serial_putchar(char c)
{
	/* Wait for line to be clear */
	while ((inb(COM1 | 5) & 0x20) == 0)
		;

	outb(COM1, c);
}

void serial_trap()
{
	while ((inb(COM1 | 5) & 0x01) == 0)
		;
	char c = inb(COM1);

	ringbuf_write(tty0->data, &c, 1);

	lapic_eoi();
}

ssize_t serial_read(char *buf)
{
	ssize_t ret;

	char c;

	struct proc *proc = proc_find(getpid());

	serial_trap();

	while ((ret = ringbuf_read(tty0->data, &c, 1)) == 0) {
		_save_context();
		sti();
		yield();
	}

	copy_to_user(proc, buf, &c, 1);

	return ret;
}

ssize_t serial_write(const char *buf, size_t count)
{
	for (size_t i = 0; i < count; i++)
		serial_putchar(buf[i]);
	return count;
}

ssize_t serial_write_dev(void *dev, void *buf, size_t offset, size_t count)
{
	return serial_write(buf, count);
}

ssize_t serial_read_dev(void *dev, void *buf, size_t offset, size_t count)
{
	return serial_read(buf);
}
