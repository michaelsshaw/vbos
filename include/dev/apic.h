#ifndef _APIC_H_
#define _APIC_H_

#define MADT_LAPIC 0
#define MADT_IOAPIC 1
#define MADT_IOAPIC_OVRD 2
#define MADT_IOAPIC_NMI 3
#define MADT_LAPIC_NMI 4
#define MADT_LAPIC_OVRD 5
#define MADT_X2_LAPIC 9

#ifndef __ASM__

#include <kernel/common.h>
#include <kernel/acpi.h>

struct madt {
	struct acpi_sdt_header h;
	uint32_t lapic_addr;
	uint32_t flags;
} PACKED;

struct madt_header {
	uint8_t type;
	uint8_t length;
} PACKED;

struct madt_lapic {
	struct madt_header h;
	uint8_t acpi_cpu_id;
	uint8_t apic_id;
	uint32_t flags;
} PACKED;

struct madt_ioapic {
	struct madt_header h;
	uint8_t ioapic_id;
	uint8_t reserved;
	uint32_t ioapic_addr;
	uint32_t intr_base;
} PACKED;

struct madt_ioapic_ovrd {
	struct madt_header h;
	uint8_t bus;
	uint8_t irq;
	uint32_t intr;
	uint16_t flags;
} PACKED;

struct madt_ioapic_nmi {
	struct madt_header h;
	uint8_t nmi;
	uint8_t reserved;
	uint16_t flags;
	uint32_t intr;
} PACKED;

struct madt_lapic_nmi {
	struct madt_header h;
	uint8_t acpi_cpu_id;
	uint16_t flags;
	uint8_t lint;
} PACKED;

struct madt_lapic_ovrd {
	struct madt_header h;
	uint16_t reserved;
	uint64_t lapic_addr;
} PACKED;

struct madt_x2_lapic {
	struct madt_header h;
	uint16_t reserved;
	uint32_t lapic_id;
	uint32_t flags;
	uint32_t acpi_id;
} PACKED;

int madt_parse_next_entry(int offset);

#endif /* __ASM__ */
#endif /* _APIC_H_ */
