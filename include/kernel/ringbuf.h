/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _RINGBUF_H
#define _RINGBUF_H

#include <kernel/common.h>

typedef struct _ringbuf {
	char *buf;
	size_t size;
	size_t head;
	size_t tail;
} ringbuf_t;

ringbuf_t *ringbuf_create(size_t size);
void ringbuf_destroy(ringbuf_t *rb);

size_t ringbuf_write(ringbuf_t *rb, const char *buf, size_t count);
size_t ringbuf_read(ringbuf_t *rb, char *buf, size_t count);

#endif /* _RINGBUF_H */
