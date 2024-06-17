/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _CHAR_H_
#define _CHAR_H_

#include <kernel/common.h>
#include <kernel/proc.h>

struct char_queue {
	char *buf;
	size_t size;
	struct proc *proc;
	struct char_queue *next;
};

#endif /* _CHAR_H_ */
