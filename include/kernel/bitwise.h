/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _BITWISE_H_
#define _BITWISE_H_

#ifndef __ASM__

#define BIT_SET(_v, _bit) ((_v) |= (1 << (_bit)))
#define BIT_CLEAR(_v, _bit) ((_v) &= (~(1 << (_bit))))
#define BIT_FLIP(_v, _bit) ((_v) ^= (1 << (_bit)))

#endif /* __ASM__ */
#endif /* _BITWISE_H_ */
