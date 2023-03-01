/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <dev/apic.h>

#include <stdio.h>

static void *madt_entries[0x20];
static int n_madt_entries = 0;

static const char *madt_entry_types[] = { "LAPIC",
					  "IOAPIC",
					  "IOAPIC_IRQ_SRC_OVRD",
					  "IOAPIC_NMI_SRC",
					  "LAPIC_NMI",
					  "LAPIC_ADDR_OVRD",
					  "INVALID",
					  "INVALID",
					  "INVALID",
					  "CPU_LOCAL_X2_APIC" };

uint32_t ioapic_read(void *addr, uint32_t reg)
{
	uint32_t volatile *ioapic = (uint32_t volatile *)addr;
	ioapic[0] = (reg & 0xFF);
	return ioapic[4];
}

void ioapic_write(void *addr, uint32_t reg, uint32_t value)
{
	uint32_t volatile *ioapic = (uint32_t volatile *)addr;
	ioapic[0] = (reg & 0xFF);
	ioapic[4] = value;
}

int madt_parse_next_entry(int offset)
{
	if ((unsigned)offset >= (__madt->h.length - 1))
		return 0;

	uint8_t *madt = (uint8_t *)__madt;
	int type = madt[offset];
	int len = madt[offset + 1];

	printf(LOG_INFO "MADT Entry %d: %s\n", n_madt_entries,
	       madt_entry_types[type]);

	/* Can't declare variables inside switch statement */
	struct madt_lapic *lapic = (struct madt_lapic *)madt;

	switch (type) {
	case MADT_LAPIC:
		printf(LOG_INFO "Lapic base address for cpu %d: %xh\n",
		       lapic->acpi_cpu_id, __madt->lapic_addr);
		break;
	case MADT_IOAPIC:
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
		printf(LOG_WARN "Unrecognized MADT entry #%d, type=%d\n",
		       n_madt_entries, type);
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

	printf(LOG_SUCCESS "APIC initialized\n");
}
