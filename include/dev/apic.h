/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _APIC_H_
#define _APIC_H_

#define MADT_LAPIC 0
#define MADT_IOAPIC 1
#define MADT_IOAPIC_OVRD 2
#define MADT_IOAPIC_NMI 3
#define MADT_LAPIC_NMI 4
#define MADT_LAPIC_OVRD 5
#define MADT_X2_LAPIC 9

#define LAPIC_ID 0x20
#define LAPIC_VERSION 0x30
#define LAPIC_TPR 0x80
#define LAPIC_APR 0x90
#define LAPIC_PPR 0xA0
#define LAPIC_EOI 0xB0
#define LAPIC_RRD 0xC0
#define LAPIC_LDR 0xD0
#define LAPIC_DFR 0xE0
#define LAPIC_SIVR 0xF0
#define LAPIC_ISR 0x100
#define LAPIC_TMR 0x180
#define LAPIC_IRR 0x200
#define LAPIC_ERR 0x280
#define LAPIC_CMCI 0x2F0
#define LAPIC_ICR 0x300
#define LAPIC_LVT_TIMER 0x320
#define LAPIC_THERMAL 0x330
#define LAPIC_LVT_PERF_MON_COUNT 0x340
#define LAPIC_LVT_LINT0 0x350
#define LAPIC_LVT_LINT1 0x360
#define LAPIC_LVT_ERR 0x370
#define LAPIC_INIT_COUNT 0x380
#define LAPIC_CUR_COUNT 0x390
#define LAPIC_DIVIDE_CONFIG 0x3E0

#define MSR_IA32_APIC_BASE 0x1B

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

struct apic_redirect {
	union {
		struct {
			uint64_t isr : 8;
			uint64_t delv_mode : 3;
			uint64_t dest_mode : 1;
			uint64_t delv_status : 1;
			uint64_t polarity : 1;
			uint64_t remote_irr : 1;
			uint64_t trigger : 1;
			uint64_t mask : 1;
			uint64_t reserved : 39;
			uint64_t dest : 8;
		} PACKED;
		struct {
			uint32_t low;
			uint32_t high;
		} PACKED;
	};
} PACKED;

void apic_init();
void lapic_eoi();
int madt_parse_next_entry(int offset);

#endif /* __ASM__ */
#endif /* _APIC_H_ */
