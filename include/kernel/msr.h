/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _MSR_H_
#define _MSR_H_

#include <kernel/common.h>

#define MSR_APIC_BASE 0x1B

#ifndef __ASM__

uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t val);

#endif /* __ASM__ */
#endif /* _MSR_H_ */
