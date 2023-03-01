/* SPDX-License-Identifier: GPL-2.0-only */
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
} PACKED;

struct acpi_sdt_header {
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

struct rsdt {
	struct acpi_sdt_header h;
	uint32_t sdt[];
} PACKED;

void acpi_init();

extern struct madt *__madt;

#endif /* __ASM__ */
#endif /* _ACPI_H_ */
