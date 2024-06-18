/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RINGBUF_H
#define _RINGBUF_H

#include <kernel/common.h>

struct ringbuf {
	char *buf;
	size_t size;
	size_t head;
	size_t tail;
};

struct ringbuf *ringbuf_create(size_t size);
void ringbuf_destroy(struct ringbuf *rb);

size_t ringbuf_write(struct ringbuf *rb, const char *buf, size_t count);
size_t ringbuf_read(struct ringbuf *rb, char *buf, size_t count);

#endif /* _RINGBUF_H */
