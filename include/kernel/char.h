/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _CHAR_H_
#define _CHAR_H_

#include <kernel/common.h>

struct char_device {
	char *name;
	void *data;
};

struct char_device *char_device_create(const char *name, void *data);
void char_device_destroy(struct char_device *cd);

#endif /* _CHAR_H_ */
