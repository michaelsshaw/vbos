/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/mem.h>
#include <kernel/irq.h>

#include <dev/ahci.h>
#include <dev/pci.h>

static hbamem_t *abar = NULL;

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

	printf(LOG_SUCCESS "AHCI controller ready\n");
}
