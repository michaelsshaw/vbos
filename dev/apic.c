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

int madt_parse_next_entry(int offset)
{
	if ((unsigned)offset >= (__madt->h.length - 1))
		return 0;

	uint8_t *madt = (uint8_t *)__madt;
	int type = madt[offset];
	int len = madt[offset + 1];

	printf("MADT Entry %d: %s\n", n_madt_entries, madt_entry_types[type]);

	switch (type) {
	case MADT_LAPIC:
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
		printf("Unrecognized MADT entry #%d, type=%d\n", n_madt_entries,
		       type);
	}

	/* Add this entry to the list */
	madt_entries[n_madt_entries] = madt;

	n_madt_entries++;
	return offset + len;
}
