/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/pio.h>
#include <kernel/slab.h>
#include <dev/pci.h>

#define MAX_PCI_DEVICES 256

static struct pci_device *pci_devices;
static uint8_t pci_device_count;

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t addr;
	uint32_t lbus = bus;
	uint32_t lslot = slot;
	uint32_t lfunc = func;
	uint16_t tmp = 0;

	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | 0x80000000;

	outl(PCIPM_CONFIG_ADDRESS, addr);
	tmp = (uint16_t)((inl(PCIPM_CONFIG_DATA) >> ((offset & 2) << 3)) & 0xFFFF);

	return tmp;
}

static void pci_scan_bus(uint8_t bus)
{
	for (uint8_t slot = 0; slot < 32; slot++) {
		uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0);
		uint16_t device_id = pci_config_read_word(bus, slot, 0, 2);
		uint8_t class = pci_config_read_word(bus, slot, 0, 0xA);
		uint8_t subclass = (pci_config_read_word(bus, slot, 0, 0xA) >> 8);

		if (vendor_id == 0xFFFF || class == 0xFF)
			continue;

		pci_devices[pci_device_count].bus = bus;
		pci_devices[pci_device_count].slot = slot;
		pci_devices[pci_device_count].vendor_id = vendor_id;
		pci_devices[pci_device_count].device_id = device_id;
		pci_devices[pci_device_count].class = class;
		pci_devices[pci_device_count].subclass = subclass;

		pci_device_count++;
	}
}

void pci_init()
{
	pci_devices = kzalloc(sizeof(struct pci_device) * MAX_PCI_DEVICES);
	pci_device_count = 0;

	for (int bus = 0; bus < 256; bus++)
		pci_scan_bus(bus);

#ifdef KDEBUG
	/* Print PCI devices */
	for (int i = 0; i < pci_device_count; i++) {
		struct pci_device *dev = &pci_devices[i];
		const char fmt[] = LOG_DEBUG "PCI device BUS[%d:%d] CLASS[%d:%d] vendor_id=%xh device_id=%xh\n";

		printf(fmt, dev->bus, dev->slot, dev->class, dev->subclass, dev->vendor_id, dev->device_id);
	}
#endif /* KDEBUG */
}
