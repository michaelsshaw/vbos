/* SPDX-License-Identifier: GPL-2.0-only */
#include "stdio.h"
#include <kernel/mem.h>
#include <kernel/irq.h>

#include <dev/ahci.h>
#include <dev/pci.h>

static hbamem_t *abar = NULL;

static void ahci_irq_handler()
{
}

static int ahci_port_type(hbaport_t *port)
{
	uint32_t ssts = port->ssts;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT)
		return AHCI_DEV_NULL;

	switch (port->sig) {
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}

static void ahci_probe_ports(hbamem_t *abar)
{
	uint32_t pi = abar->pi;
	for (int i = 0; i < 32; i++) {
		if (pi & 1) {
			uint32_t type = ahci_port_type(&abar->ports[i]);
			if (type == AHCI_DEV_SATA) {
				printf(LOG_INFO "SATA drive found at port %d\n", i);
			} else if (type == AHCI_DEV_SATAPI) {
				printf(LOG_INFO "SATAPI drive found at port %d\n", i);
			} else if (type == AHCI_DEV_SEMB) {
				printf(LOG_INFO "SEMB drive found at port %d\n", i);
			} else if (type == AHCI_DEV_PM) {
				printf(LOG_INFO "PM drive found at port %d\n", i);
			}
		}
		pi >>= 1;
	}
}

void ahci_init()
{
	struct pci_device *dev = pci_find_device(0x01, 0x06);
	/* check interrupt number */
	if (dev == NULL) {
		printf(LOG_ERROR "Failed to find AHCI device\n");
		return;
	}
	abar = (hbamem_t *)((uintptr_t)dev->bar5 | hhdm_start);

	if (!(abar->ghc & (1 << 31))) {
		printf(LOG_ERROR "AHCI controller not enabled\n");
		return;
	}

	ahci_probe_ports(abar);

	/* setup device irq number */
	uint16_t irq = irq_highest_free();
	uint32_t tmp = pci_config_read_long(dev->bus, dev->slot, 0, 0x3C);
	tmp &= 0xFFFFFF00;
	tmp |= irq;
	pci_config_write_long(dev->bus, dev->slot, 0, 0x3C, tmp);
	irq_map(irq, ahci_irq_handler);

	tmp = pci_config_read_long(dev->bus, dev->slot, 0, 0x3C);

	printf(LOG_SUCCESS "AHCI controller ready\n");
}
