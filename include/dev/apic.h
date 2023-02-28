#ifndef _APIC_H_
#define _APIC_H_

#ifndef __ASM__

#include <kernel/common.h>

struct madt {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oemid[6];
	uint64_t oem_tableid;
	uint32_t oem_rev;
	uint32_t creator_id;
	uint32_t creator_revision;

} __attribute__((packed));

#endif /* __ASM__ */
#endif /* _APIC_H_ */
