/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _PCI_H_
#define _PCI_H_

#include <kernel/common.h>

#define PCIPM_CONFIG_ADDRESS 0xCF8
#define PCIPM_CONFIG_DATA 0xCFC

struct pci_device {
	uint8_t bus;
	uint8_t slot;

	uint8_t class;
	uint8_t subclass;

	uint16_t vendor_id;
	uint16_t device_id;

};

void pci_init();
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif /* _PCI_H_ */
