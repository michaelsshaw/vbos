#include <kernel/acpi.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <dev/apic.h>

#include <stdint.h>
#include <stdio.h>

static void *madt_entries[0x20];
static int n_madt_entries = 0;

static uintptr_t ioapic_addr = 0;
static uintptr_t lapic_addr = 0;
static uint8_t lapic_id = 0;

static const char *madt_entry_types[] = {
	"LAPIC",   "IOAPIC",  "IOAPIC_IRQ_SRC_OVRD", "IOAPIC_NMI_SRC", "LAPIC_NMI", "LAPIC_ADDR_OVRD", "INVALID",
	"INVALID", "INVALID", "CPU_LOCAL_X2_APIC"
};

inline uint32_t ioapic_read(uintptr_t addr, uint8_t reg)
{
	*(volatile uint32_t *)(addr) = reg;
	return *(volatile uint32_t *)(addr + 0x10);
}

inline void ioapic_write(uintptr_t addr, uint8_t reg, uint32_t value)
{
	*(volatile uint32_t *)(addr) = reg;
	*(volatile uint32_t *)(addr + 0x10) = value;
}

void ioapic_redirect_insert(uint8_t irq, uint8_t isr, uint8_t low_active, uint8_t level_trigger)
{
	int entryno = 0x10 + irq * 2;
	struct apic_redirect entry;
	entry.isr = isr;
	entry.delv_mode = 0;
	entry.dest_mode = 0;
	entry.polarity = low_active;
	entry.trigger = level_trigger;
	entry.mask = 0;
	entry.dest = lapic_id;

	ioapic_write(ioapic_addr, entryno, entry.low);
	ioapic_write(ioapic_addr, entryno + 1, entry.high);

	entryno += 2;
}

inline uint32_t lapic_read(uintptr_t addr, unsigned int regoffset)
{
	volatile uint32_t *lapic = (volatile uint32_t *)addr;
	return lapic[regoffset >> 2];
}

inline uint32_t lapic_write(uintptr_t addr, unsigned int regoffset, uint32_t value)
{
	volatile uint32_t *lapic = (volatile uint32_t *)addr;
	lapic[regoffset >> 2] = value;
	return lapic[regoffset >> 2];
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
	struct madt_lapic *m_lapic = (struct madt_lapic *)madt;

	switch (type) {
	case MADT_LAPIC:
		lapic_id = m_lapic->apic_id;
		printf(LOG_INFO "LAPIC ID: %xh\n", m_lapic->apic_id);
		break;
	case MADT_IOAPIC:
		ioapic_addr = m_ioapic->ioapic_addr | hhdm_start;
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

void apic_eoi()
{
	lapic_write(lapic_addr, 0xB0, 0);
}

void apic_init()
{
	if (__madt == NULL) {
		printf(LOG_WARN "Failed to locate MADT\n");
		return;
	}

	int offset = 0x2C;
	int length = __madt->h.length;
	printf(LOG_INFO "MADT Length: %x\n", length);

	while ((offset = madt_parse_next_entry(offset)))
		;

	lapic_addr = __madt->lapic_addr | hhdm_start;

	printf(LOG_INFO "IOAPIC ADDR: %X\n", ioapic_addr);
	printf(LOG_INFO "LAPIC ADDR : %X\n", lapic_addr);

	uint32_t r = ioapic_read(ioapic_addr, 0);
	r |= 0x04000000;

	ioapic_write(ioapic_addr, 0, r);
	printf(LOG_WARN "IOAPIC ID:%x\n", r);

	printf(LOG_WARN "LAPIC_ID:%xh\n", lapic_id);
	ioapic_redirect_insert(4, 0x21, 0, 0);
	/* Set spurious interrupt vector to 0xFF and enable the LAPIC */
	lapic_id = lapic_write(lapic_addr, 0xF0, 0x1FF);
	printf(LOG_WARN "%x\n", lapic_id);

	printf(LOG_SUCCESS "APIC initialized\n");
}
