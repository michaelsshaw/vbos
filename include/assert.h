/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASSERT_H_
#define _ASSERT_H_

#ifdef KDEBUG
#define assert(expr)                                                                                              \
	do {                                                                                                      \
		if (!(expr)) {                                                                                    \
			kprintf(LOG_ERROR "assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
			yield();                                                                                  \
		}                                                                                                 \
	} while (0)
#else
#define assert(expr) ((void)0)
#endif

#endif /* _ASSERT_H_ */
