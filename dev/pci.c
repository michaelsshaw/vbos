/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/pio.h>
#include <kernel/slab.h>
#include <dev/pci.h>

#define MAX_PCI_DEVICES 256

static struct pci_device *pci_devices;
static uint8_t pci_device_count;

uint32_t pci_config_read_long(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t addr;
	uint32_t lbus = bus;
	uint32_t lslot = slot;
	uint32_t lfunc = func;

	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | 0x80000000;

	outl(PCIPM_CONFIG_ADDRESS, addr);
	return inl(PCIPM_CONFIG_DATA);
}

void pci_config_write_long(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data)
{
	uint32_t addr;
	uint32_t lbus = bus;
	uint32_t lslot = slot;
	uint32_t lfunc = func;

	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | 0x80000000;

	outl(PCIPM_CONFIG_ADDRESS, addr);
	outl(PCIPM_CONFIG_DATA, data);
}

static struct pci_device *pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func)
{
	uint32_t vendor_device = pci_config_read_long(bus, slot, func, 0x0);
	uint16_t vendor_id = vendor_device & 0xFFFF;
	uint16_t device_id = vendor_device >> 16;

	uint32_t class_subclass = pci_config_read_long(bus, slot, func, 0x8);
	uint8_t class = (class_subclass >> 24) & 0xFF;
	uint8_t subclass = (class_subclass >> 16) & 0xFF;

	if (vendor_id == 0xFFFF || class == 0xFF)
		return NULL;

	pci_devices[pci_device_count].bus = bus;
	pci_devices[pci_device_count].slot = slot;
	pci_devices[pci_device_count].vendor_id = vendor_id;
	pci_devices[pci_device_count].device_id = device_id;
	pci_devices[pci_device_count].class = class;
	pci_devices[pci_device_count].subclass = subclass;

	pci_devices[pci_device_count].bar0 = pci_config_read_long(bus, slot, func, 0x10);
	pci_devices[pci_device_count].bar1 = pci_config_read_long(bus, slot, func, 0x14);

	uint8_t header_type = (pci_config_read_long(bus, slot, 0, 0xC) >> 16) & 0xFF;
	pci_devices[pci_device_count].header_type = header_type;

	if ((header_type & 0x0F) == 0) {
		pci_devices[pci_device_count].bar2 = pci_config_read_long(bus, slot, func, 0x18);
		pci_devices[pci_device_count].bar3 = pci_config_read_long(bus, slot, func, 0x1C);
		pci_devices[pci_device_count].bar4 = pci_config_read_long(bus, slot, func, 0x20);
		pci_devices[pci_device_count].bar5 = pci_config_read_long(bus, slot, func, 0x24);
	}

	pci_device_count++;

	return &pci_devices[pci_device_count - 1];
}

static void pci_scan_bus(uint8_t bus)
{
	for (uint8_t slot = 0; slot < 32; slot++) {
		struct pci_device *dev = pci_scan_function(bus, slot, 0);
		if (dev != NULL && dev->header_type & 0x80) {
			for (uint8_t func = 1; func < 8; func++) {
				if (pci_config_read_long(bus, slot, func, 0) != 0xFFFFFFFF)
					pci_scan_function(bus, slot, func);
			}
		}
	}
}

struct pci_device *pci_find_device(uint8_t class, uint8_t subclass)
{
	for (int i = 0; i < pci_device_count; i++) {
		if (pci_devices[i].class == class && pci_devices[i].subclass == subclass)
			return &pci_devices[i];
	}

	return NULL;
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
		const char fmt[] = LOG_DEBUG "PCI device BUS[%d:%d] CLASS[%d:%d] vendor_id=%xh device_id=%xh, header_type=%d\n";

		printf(fmt, dev->bus, dev->slot, dev->class, dev->subclass, dev->vendor_id, dev->device_id, dev->header_type);
	}
#endif /* KDEBUG */
}
