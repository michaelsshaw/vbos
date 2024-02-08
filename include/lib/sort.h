/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _SORT_H_
#define _SORT_H_

#include <stddef.h>

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif /* _SORT_H_ */
