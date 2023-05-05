/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/mem.h>
#include <kernel/irq.h>

#include <dev/ahci.h>
#include <dev/pci.h>

static hbamem_t *abar = NULL;

static void ahci_irq_handler()
{
}

void ahci_init()
{
	struct pci_device *dev = pci_find_device(0x01, 0x06);
	if (dev == NULL) {
		printf(LOG_ERROR "Failed to find AHCI device\n");
		return;
	}

	abar = (hbamem_t *)((uintptr_t)dev->bar5 | hhdm_start);

	if (!(abar->ghc & (1 << 31))) {
		printf(LOG_ERROR "AHCI controller not enabled\n");
		return;
	}

	/* setup device irq number */
	uint8_t irq = irq_highest_free();
	uint16_t tmp = pci_config_read_word(dev->bus, dev->slot, 0, 0x3C);
	tmp &= 0xFF00;
	tmp |= irq;
	pci_config_write_word(dev->bus, dev->slot, 0, 0x3C, tmp);
	irq_map(irq, ahci_irq_handler);

	printf(LOG_SUCCESS "AHCI controller ready\n");
}
