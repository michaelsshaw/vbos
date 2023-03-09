/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <kernel/mem.h>
#include <dev/apic.h>

#include <stdint.h>
#include <stdio.h>

static void *madt_entries[0x20];
static int n_madt_entries = 0;
static uintptr_t ioapic_addr = 0;

static const char *madt_entry_types[] = {
	"LAPIC",   "IOAPIC",  "IOAPIC_IRQ_SRC_OVRD", "IOAPIC_NMI_SRC", "LAPIC_NMI", "LAPIC_ADDR_OVRD", "INVALID",
	"INVALID", "INVALID", "CPU_LOCAL_X2_APIC"
};

uint32_t ioapic_read(uintptr_t addr, uint8_t reg)
{
	*(volatile uint32_t *)(addr) = reg;
	return *(volatile uint32_t *)(addr + 0x10);
}

void ioapic_write(uintptr_t addr, uint8_t reg, uint32_t value)
{
	*(volatile uint32_t *)(addr) = reg;
	*(volatile uint32_t *)(addr + 0x10) = value;
}

int madt_parse_next_entry(int offset)
{
	if ((unsigned)offset >= (__madt->h.length - 1))
		return 0;

	uint8_t *madt = (uint8_t *)__madt;
	int type = madt[offset];
	int len = madt[offset + 1];

	printf(LOG_INFO "MADT Entry %d: %s\n", n_madt_entries, madt_entry_types[type]);

	/* Can't declare variables inside switch statement */
	struct madt_ioapic *m_ioapic = (struct madt_ioapic *)madt;

	switch (type) {
	case MADT_LAPIC:
		break;
	case MADT_IOAPIC:
		ioapic_addr = m_ioapic->ioapic_addr;
		printf(LOG_INFO "IOAPIC address: %xh\n", m_ioapic->ioapic_addr);
		break;
	case MADT_IOAPIC_OVRD:
		break;
	case MADT_IOAPIC_NMI:
		break;
	case MADT_LAPIC_NMI:
		break;
	case MADT_LAPIC_OVRD:
		break;
	case MADT_X2_LAPIC:
		break;
	default:
		printf(LOG_WARN "Unrecognized MADT entry #%d, type=%d\n", n_madt_entries, type);
		break;
	}

	/* Add this entry to the list */
	madt_entries[n_madt_entries] = madt;

	n_madt_entries++;
	return offset + len;
}

void apic_init()
{
	if (__madt == NULL) {
		printf(LOG_WARN "Failed to locate MADT\n");
	} else {
		/* First variable entry in MADT */
		int offset = 0x2C;
		int length = __madt->h.length;
		printf(LOG_INFO "MADT Length: %x\n", length);

		while ((offset = madt_parse_next_entry(offset)))
			;
	}

	ioapic_addr += hhdm_start;
	printf(LOG_INFO "IOAPIC ADDR: %X\n", ioapic_addr);



	printf(LOG_SUCCESS "APIC initialized\n");
}
