#ifndef _ACPI_H_
#define _ACPI_H_

#ifndef __ASM__

#include <kernel/common.h>

struct rsdp {
	char signature[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t revision;
	uint32_t rsdt_addr;

	uint32_t length;
	uint64_t xsdt_addr; /* the REAL address */
	uint8_t checksum_ext;
	char reserved[3];
} PACKED;

struct acpi_header {
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char oem_id[6];
	char oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} PACKED;

void acpi_init();

#endif /* __ASM__ */
#endif /* _ACPI_H_ */
