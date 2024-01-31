#include <kernel/acpi.h>
#include <kernel/mem.h>
#include <kernel/msr.h>
#include <dev/apic.h>

#include <stdint.h>

static uintptr_t ioapic_addr = 0xFEC00000;
static uintptr_t lapic_addr = 0;
static uint8_t lapic_id = 0;

static struct ioapic {
	uint32_t reg;
	uint32_t pad[3];
	uint32_t data;
} volatile *ioapic;

uint32_t ioapic_read(uint32_t reg)
{
	ioapic->reg = reg;
	return ioapic->data;
}

void ioapic_write(uint32_t reg, uint32_t value)
{
	ioapic->reg = reg;
	ioapic->data = value;
}

void ioapic_redirect_insert(size_t irq, size_t isr, size_t low_active, size_t level_trigger)
{
	/* Set the interrupt redirection entry */
	ioapic_write(0x10 + 2 * irq, (level_trigger << 15) | (low_active << 13) | isr);
	ioapic_write(0x10 + 2 * irq + 1, lapic_id << 24);
}

uint32_t lapic_read(uintptr_t addr, unsigned int regoffset)
{
	volatile uint32_t *lapic = (volatile uint32_t *)addr;
	return lapic[regoffset >> 2];
}

uint32_t lapic_write(uintptr_t addr, unsigned int regoffset, uint32_t value)
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

	/* Can't declare variables inside switch statement */
	struct madt_lapic *m_lapic = (struct madt_lapic *)madt;

	switch (type) {
	case MADT_LAPIC:
		lapic_id = m_lapic->acpi_cpu_id;
	default:
		break;
	}

	return offset + len;
}

void apic_eoi()
{
	lapic_write(lapic_addr, 0xB0, 0);
}

void  lapic_eoi()
{
	lapic_write(lapic_addr, LAPIC_EOI, 0);
}

void lapic_enable()
{
	/* Set spurious interrupt vector to 0xFF and enable the LAPIC */
	lapic_write(lapic_addr, 0xF0, 0x1FF);
}

uint8_t lapic_idno()
{
	return lapic_read(lapic_addr, LAPIC_ID) >> 24;
}

void apic_init()
{
	if (__madt == NULL) {
		kprintf(LOG_WARN "No MADT found\n");
		return;
	}

	int offset = 0x2C;

	while ((offset = madt_parse_next_entry(offset)))
		;

	lapic_addr = __madt->lapic_addr;

	lapic_addr = (uintptr_t)kmap_device((void *)lapic_addr, 0x1000);
	ioapic_addr = (uintptr_t)kmap_device((void *)ioapic_addr, 0x1000);

	ioapic = (void *)ioapic_addr;

	uint32_t r = ioapic_read(0);
	r |= 0x04000000;
	ioapic_write(0, r);

	/* get ioapic version */
	lapic_id = lapic_read(lapic_addr, 0x20);
	ioapic_redirect_insert(4, 0x24, 0, 0);

	lapic_enable();

	uint64_t msr = rdmsr(MSR_IA32_APIC_BASE);
	msr |= 1 << 11;
	wrmsr(MSR_IA32_APIC_BASE, msr);

	lapic_eoi();

	kprintf(LOG_SUCCESS "APIC initialized\n");
}
