/* SPDX-License-Identifier: GPL-2.0-only */

#include <kernel/common.h>
#include <kernel/ringbuf.h>
#include <kernel/slab.h>

#include <kernel/char.h>

struct char_device *char_device_create(const char *name, void *data)
{
	size_t name_len = strlen(name);
	struct char_device *cd = kmalloc(sizeof(struct char_device), ALLOC_KERN);
	cd->name = kzalloc(name_len + 1, ALLOC_KERN);
	cd->data = data;
	return cd;
}

void char_device_destroy(struct char_device *cd)
{
	ATTEMPT_FREE(cd->name);
	ATTEMPT_FREE(cd);
}
