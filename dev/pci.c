/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/pio.h>
#include <dev/pci.h>

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
	uint32_t addr;
	uint32_t lbus = bus;
	uint32_t lslot = slot;
	uint32_t lfunc = func;
	uint16_t tmp = 0;

	addr = (lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000);

	outl(PCIPM_CONFIG_ADDRESS, addr);
	tmp = (uint16_t)((inl(PCIPM_CONFIG_DATA) >> ((offset & 2) << 3)) & 0xFFFF);

	return tmp;
}

static void pci_print_bus(uint8_t bus)
{
	for (uint8_t slot = 0; slot < 32; slot++) {
		uint16_t vendor_id = pci_config_read_word(bus, slot, 0, 0);
		if (vendor_id == 0xFFFF)
			continue;

		uint8_t class = pci_config_read_word(bus, slot, 0, 0xA);
		uint8_t subclass = (pci_config_read_word(bus, slot, 0, 0xA) >> 8);
		uint8_t prog_if = (pci_config_read_word(bus, slot, 0, 0x8) >> 8);

		printf(LOG_INFO "PCI device: vendor:%xh class:%xh subclass:%xh prof_if:%xh\n", vendor_id, class, subclass, prog_if);
	}
}

void pci_init()
{
	for (int bus = 0; bus < 256; bus++)
		pci_print_bus(bus);
}
