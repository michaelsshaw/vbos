/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/block.h>
#include <kernel/irq.h>
#include <kernel/pio.h>

#include <dev/ahci.h>
#include <dev/pci.h>

static int ahci_block_read(struct block_device *dev, void *buf, size_t offset, size_t sector_count);
static int ahci_block_write(struct block_device *dev, void *buf, size_t offset, size_t sector_count);

static struct block_device_ops ahci_block_ops = {
	.read = ahci_block_read,
	.write = ahci_block_write,
};

static hbamem_t *abar = NULL;

static uint8_t sata_device_count = 0;
static struct sata_device sata_devices[32] = { 0 };

static paddr_t ahci_pbase = 0;

static void ahci_irq_handler()
{
	printf(LOG_ERROR "AHCI INTERRUPT\n");
}

static int ahci_port_type(hbaport_t *port)
{
	uint32_t ssts = port->ssts;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT)
		return AHCI_DEV_NULL;

	if (port->clb == 0)
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

static void halt_cmd(hbaport_t *port)
{
	port->cmd &= ~HBA_PORT_CMD_ST;
	port->cmd &= ~HBA_PORT_CMD_FRE;

	while (port->cmd & (HBA_PORT_CMD_FR | HBA_PORT_CMD_CR))
		;
}

static void start_cmd(hbaport_t *port)
{
	while (port->cmd & HBA_PORT_CMD_CR)
		;

	port->cmd |= HBA_PORT_CMD_FRE;
	port->cmd |= HBA_PORT_CMD_ST;
}

static void ahci_rebase(hbaport_t *port, int n)
{
	halt_cmd(port);

	port->clb = (ahci_pbase) + (n << 10);
	memset((void *)(port->clb | hhdm_start), 0, 1024);

	port->fb = ahci_pbase + (32 << 10) + (n << 8);
	memset((void *)(port->fb | hhdm_start), 0, 256);

	struct hba_cmd_header *cmdheader = (struct hba_cmd_header *)(port->clb | hhdm_start);
	for (int i = 0; i < 32; i++) {
		cmdheader[i].prdt_len = 8;
		cmdheader[i].cmd_table_base = (ahci_pbase) + (n << 13) + (40 << 10) + (i << 8);

		void *ctba = (void *)((uintptr_t)cmdheader[i].cmd_table_base | hhdm_start);
		memset(ctba, 0, 256);
	}

	start_cmd(port);
}

static void ahci_probe_ports(struct pci_device *dev, hbamem_t *abar)
{
	uint32_t pi = abar->pi;
	for (int i = 0; i < 32; i++) {
		if (pi & 1) {
			uint32_t type = ahci_port_type(&abar->ports[i]);
			if (type == AHCI_DEV_SATA) {
				struct sata_device *sdev = &sata_devices[sata_device_count];
				sdev->port = i;
				sdev->controller = dev;
				sdev->abar = abar;

				char *name = kzalloc(32, ALLOC_KERN);
				snprintf(name, "ahci%d", 32, sata_device_count);
				block_register(name, &ahci_block_ops, sdev);

				ahci_rebase(&abar->ports[i], i);
				sata_device_count++;
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

static int cmdslot(hbaport_t *port)
{
	uint32_t slots = (port->sact | port->ci);
	for (int i = 0; i < 32; i++) {
		if ((slots & 1))
			slots >>= 1;
		else
			return i;
	}

	return -1;
}

bool ahci_access_sectors(struct sata_device *dev, paddr_t lba, uint16_t count, paddr_t buf, bool write)
{
	hbaport_t *port = &dev->abar->ports[dev->port];
	uint16_t ocount = count;

	/* clear pending interrupt */
	port->is = (uint32_t)-1;

	int spin = 0;
	int slot = cmdslot(port);
	if (slot == -1)
		return false;

	struct hba_cmd_header *cmdheader = (struct hba_cmd_header *)(port->clb | hhdm_start);
	cmdheader += slot;
	cmdheader->cmd_fis_len = sizeof(struct fis_reg_h2d) / sizeof(uint32_t);
	cmdheader->write = 0;
	cmdheader->prdt_len = (uint16_t)((count - 1) >> 4) + 1;

	struct hba_cmd_tbl *cmdtbl = (struct hba_cmd_tbl *)(cmdheader->cmd_table_base | hhdm_start);
	memset(cmdtbl, 0, sizeof(struct hba_cmd_tbl) + (cmdheader->prdt_len - 1) * sizeof(struct hba_prdt_entry));

	struct fis_reg_h2d *cmdfis = (struct fis_reg_h2d *)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_REG_H2D;
	cmdfis->cmd_mode = 1;
	cmdfis->command = write ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX;

	cmdfis->lba0 = (uint8_t)lba;
	cmdfis->lba1 = (uint8_t)(lba >> 8);
	cmdfis->lba2 = (uint8_t)(lba >> 16);
	cmdfis->device = 1 << 6; /* LBA mode */
	cmdfis->lba3 = (uint8_t)(lba >> 24);
	cmdfis->lba4 = (uint8_t)(lba >> 32);
	cmdfis->lba5 = (uint8_t)(lba >> 40);

	cmdfis->count_low = count & 0xff;
	cmdfis->count_high = (count >> 8) & 0xff;

	int i;
	for (i = 0; i < cmdheader->prdt_len; i++) {
		cmdtbl->prdt_entry[i].data_base = buf;
		cmdtbl->prdt_entry[i].byte_count = 8 * 1024 - 1;
		cmdtbl->prdt_entry[i].interrupt = 1;
		buf += 8 * 1024;
		count -= 16;
	}

	/* wait until port is free */
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
		spin++;
	}

	if (spin == 1000000) {
		printf(LOG_ERROR "AHCI: Port is hung\n");
		return false;
	}

	/* issue command */
	port->ci = 1 << slot;

	/* wait for completion */
	while (1) {
		if ((port->ci & (1 << slot)) == 0)
			break;
		if (port->is & HBA_PORT_IS_TFES) {
			printf(LOG_ERROR "AHCI: Read disk error\n");
			return false;
		}
	}

	/* check again */
	if (port->is & HBA_PORT_IS_TFES) {
		printf(LOG_ERROR "AHCI: Read disk error\n");
		return false;
	}

	/* disk read success */
#ifdef KDEBUG
	printf(LOG_DEBUG "Read %d sectors from disk\n", ocount);
#endif
	return true;
}

static int ahci_block_read(struct block_device *dev, void *buf, size_t offset, size_t sector_count)
{
	struct sata_device *sdev = (struct sata_device *)dev->data;

	/* repeat every 64K, due to the current limitations of this driver */
	while (sector_count > 0) {
		size_t n = MIN(sector_count, 128u);
		if (!ahci_access_sectors(sdev, offset, n, (paddr_t)buf & ~hhdm_start, false))
			return -1;
		if(sector_count <= 128)
			break;
		sector_count -= 128ull;
		offset += 128;
		buf += (128 << 9);
	}

	return 0;
}

static int ahci_block_write(struct block_device *dev, void *buf, size_t offset, size_t sector_count)
{
	struct sata_device *sdev = (struct sata_device *)dev->data;

	/* repeat every 64K, same as above */
	while (sector_count > 0) {
		size_t n = MIN(sector_count, 128u);
		if (!ahci_access_sectors(sdev, offset, n, (paddr_t)buf & ~hhdm_start, true))
			return -1;
		if(sector_count <= 128)
			break;
		sector_count -= 128ull;
		offset += 128;
		buf += (128 << 9);
	}

	return 0;
}

void ahci_init()
{
	struct pci_device *dev = pci_find_device(0x01, 0x06);
	/* check interrupt number */
	if (dev == NULL) {
		printf(LOG_WARN "No SATA controllers found\n");
		return;
	}
	abar = (hbamem_t *)((uintptr_t)dev->bar5 | hhdm_start);


	if (!(abar->ghc & (1 << 31))) {
		printf(LOG_WARN "AHCI controller not enabled\n");
		return;
	}

	/* enable AHCI interrupts */
	abar->ghc |= AHCI_GHC_IE;
	abar->ghc |= AHCI_GHC_AE;

	size_t ahci_zone_size = 0x100000;
	ahci_pbase = (paddr_t)buddy_alloc(ahci_zone_size) & (~hhdm_start); /* 1 MiB */
	ahci_probe_ports(dev, abar);

	/* map ahci zone with cache disabled (pcd=1) */
	struct page attrs;
	attrs.val = 1;
	attrs.rw = 1;
	attrs.pcd = 1;
	kmap(ahci_pbase, ahci_pbase | hhdm_start, ahci_zone_size, attrs.val);

	if (sata_device_count == 0) {
		printf(LOG_WARN "No SATA devices found\n");
		return;
	}

	/* setup device irq number */
	uint16_t irq = irq_highest_free();
	uint32_t tmp = pci_config_read_long(dev->bus, dev->slot, dev->func, 0x3C);
	tmp &= 0xFFFFFF00;
	tmp |= irq;
	pci_config_write_long(dev->bus, dev->slot, dev->func, 0x3C, tmp);
	irq_map(irq, ahci_irq_handler);

	printf(LOG_SUCCESS "AHCI controller ready\n");
}
