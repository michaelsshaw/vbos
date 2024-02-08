/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/slab.h>

static void swap(void *a, void *b, size_t size)
{
	char tmp[size];
	memcpy(tmp, a, size);
	memcpy(a, b, size);
	memcpy(b, tmp, size);
}

/* Hoare partition scheme */
static size_t partition(void *base, size_t n, size_t size, size_t left, size_t right, int (*compar)(const void *, const void *))
{
	void *pivot = base + left * size;
	size_t left_idx = left - 1;
	size_t right_idx = right + 1;

	while (1) {
		do {
			left_idx++;
		} while (compar((void *)base + (left_idx * size), pivot) < 0);

		do {
			right_idx--;
		} while (compar((void *)base + (right_idx * size), pivot) > 0);

		if (left_idx >= right_idx)
			return right_idx;

		swap((void *)base + (left_idx * size), (void *)base + (right_idx * size), n);
	}
}

static void _qsort(void *base, size_t size, size_t left, size_t right, int (*compar)(const void *, const void *))
{
	if (left >= right)
		return;

	size_t pivot = partition(base, size, size, left, right, compar);
	_qsort(base, size, left, pivot, compar);
	_qsort(base, size, pivot + 1, right, compar);
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	_qsort(base, size, 0, nmemb - 1, compar);
}
