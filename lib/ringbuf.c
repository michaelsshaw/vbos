/* SPDX-License-Identifier: GPL-2.0-only */
#include "kernel/slab.h"
#include <kernel/common.h>
#include <kernel/ringbuf.h>

ringbuf_t *ringbuf_create(size_t size)
{
	ringbuf_t *rb = kzalloc(sizeof(ringbuf_t), ALLOC_KERN);
	rb->buf = kmalloc(size, ALLOC_KERN);
	rb->size = size;
	rb->head = 0;
	rb->tail = 0;
	return rb;
}

void ringbuf_destroy(ringbuf_t *rb)
{
	ATTEMPT_FREE(rb->buf);
	ATTEMPT_FREE(rb);
}

size_t ringbuf_write(ringbuf_t *rb, const char *buf, size_t count)
{
	size_t written = 0;
	while (count--) {
		if ((rb->head + 1) % rb->size == rb->tail)
			break;
		rb->buf[rb->head] = *buf++;
		rb->head = (rb->head + 1) % rb->size;
		written++;
	}
	return written;
}

size_t ringbuf_read(ringbuf_t *rb, char *buf, size_t count)
{
	size_t read = 0;
	while (count--) {
		if (rb->head == rb->tail)
			break;
		*buf++ = rb->buf[rb->tail];
		rb->tail = (rb->tail + 1) % rb->size;
		read++;
	}
	return read;
}
