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

	uint8_t header_type;

	paddr32_t bar0;
	paddr32_t bar1;
	paddr32_t bar2;
	paddr32_t bar3;
	paddr32_t bar4;
	paddr32_t bar5;
};

struct pci_header_base {
	uint16_t vendor_id;
	uint16_t device_id;

	uint16_t command;
	uint16_t status;

	uint8_t revision_id;
	uint8_t prog_if;
	uint8_t subclass;
	uint8_t class;

	uint8_t cache_line_size;
	uint8_t latency_timer;
	uint8_t header_type;
	uint8_t bist;
} PACKED;

struct pci_header_type_0 {
	struct pci_header_base base;

	paddr32_t bar0;
	paddr32_t bar1;
	paddr32_t bar2;
	paddr32_t bar3;
	paddr32_t bar4;
	paddr32_t bar5;

	uint32_t cardbus_cis_pointer;

	uint16_t subsystem_vendor_id;
	uint16_t subsystem_id;

	uint32_t expansion_rom_base_address;

	uint8_t capabilities_pointer;
	uint8_t reserved[7];

	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint8_t min_grant;
	uint8_t max_latency;
} PACKED;

struct pci_header_type_1 {
	struct pci_header_base base;

	paddr32_t bar0;
	paddr32_t bar1;

	uint8_t primary_bus_number;
	uint8_t secondary_bus_number;
	uint8_t subordinate_bus_number;
	uint8_t secondary_latency_timer;

	uint8_t io_base;
	uint8_t io_limit;
	uint16_t secondary_status;

	uint16_t memory_base;
	uint16_t memory_limit;

	uint16_t prefetchable_memory_base;
	uint16_t prefetchable_memory_limit;

	uint32_t prefetchable_base_upper;
	uint32_t prefetchable_limit_upper;

	uint16_t io_base_upper;
	uint16_t io_limit_upper;

	uint8_t capabilities_pointer;
	uint8_t reserved[3];

	uint32_t expansion_rom_base_address;

	uint8_t interrupt_line;
	uint8_t interrupt_pin;
	uint16_t bridge_control;
} PACKED;

void pci_init();
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t data);
struct pci_device *pci_find_device(uint8_t class, uint8_t subclass);

#endif /* _PCI_H_ */
