/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _AHCI_H_
#define _AHCI_H_

#include <kernel/common.h>
#include <dev/pci.h>

/* Frame information structure (FIS) */
#define FIS_REG_H2D 0x27
#define FIS_REG_D2H 0x34
#define FIS_DMA_ACT 0x39
#define FIS_DMA_SETUP 0x41
#define FIS_DATA 0x46
#define FIS_BIST 0x58
#define FIS_PIO_SETUP 0x5F
#define FIS_DEV_BITS 0xA1

#define ATA_CMD_IDENT 0xEC

#define SATA_SIG_ATA 0x00000101
#define SATA_SIG_ATAPI 0xEB140101
#define SATA_SIG_SEMB 0xC33C0101
#define SATA_SIG_PM 0x96690101

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM 3
#define AHCI_DEV_SATAPI 4

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

struct fis_reg_h2d {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 3;
	uint8_t cmd_mode : 1; /* high: command, low: control */

	uint8_t command;
	uint8_t feature_low;

	/* 0x04 */
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;

	/* 0x08 */
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t feature_high;

	/* 0x0C */
	uint8_t count_low;
	uint8_t count_high;
	uint8_t icc;
	uint8_t control;

	/* 0x10 */
	uint32_t reserved2;
} PACKED;

struct fis_reg_d2h {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 2;
	uint8_t intr : 1;
	uint8_t reserved2 : 1;

	uint8_t status;
	uint8_t error;

	/* 0x04 */
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;

	/* 0x08 */
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t feature_high;

	/* 0x0C */
	uint8_t count_low;
	uint8_t count_high;
	uint8_t icc;
	uint8_t control;

	/* 0x10 */
	uint32_t reserved3;
} PACKED;

struct fis_data {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 4;

	uint8_t reserved2[2];

	/* 0x04 */
	uint32_t data[1]; /* Variable length */
} PACKED;

struct fis_dma_setup {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 1;
	uint8_t data_dir : 1;
	uint8_t intr : 1;
	uint8_t auto_act : 1;

	uint16_t reserved2;

	/* 0x04 */
	uint64_t dma_buffer_id;

	/* 0x0C */
	uint32_t reserved3;

	/* 0x10 */
	uint32_t dma_buffer_offset;

	/* 0x14 */
	uint32_t transfer_count;

	/* 0x18 */
	uint32_t reserved4;
} PACKED;

struct fis_pio_setup {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 1;
	uint8_t data_dir : 1;
	uint8_t intr : 1;
	uint8_t reserved2 : 1;

	uint8_t status;
	uint8_t error;

	/* 0x04 */
	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;

	/* 0x08 */
	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t reserved3;

	/* 0x0C */
	uint8_t count_low;
	uint8_t count_high;
	uint8_t reserved4;
	uint8_t e_status;

	/* 0x10 */
	uint16_t transfer_count;
	uint16_t reserved5;
} PACKED;

struct fis_set_device_bits {
	/* 0x00 */
	uint8_t fis_type;

	uint8_t pmult : 4;
	uint8_t reserved : 2;
	uint8_t interrupt : 1;
	uint8_t n : 1;

	uint8_t statusl : 3;
	uint8_t reserved2 : 1;
	uint8_t statush : 3;
	uint8_t reserved3 : 1;

	/* 0x04 */
	uint8_t errorl : 3;
} PACKED;

struct hba_cmd_header {
	uint8_t cmd_fis_len : 5;
	uint8_t atapi : 1;
	uint8_t write : 1;
	uint8_t prefetch: 1;

	uint8_t reset : 1;
	uint8_t bist : 1;
	uint8_t clear_busy : 1;
	uint8_t reserved : 1;
	uint8_t pmp : 4;

	uint16_t prdt_len;

	volatile uint32_t prdb_count;

	paddr_t cmd_table_base;

	uint32_t reserved2[4];
} PACKED;

struct hba_prdt_entry {
	paddr_t data_base;

	uint32_t reserved;

	uint32_t byte_count : 22;
	uint32_t reserved2 : 9;
	uint32_t interrupt : 1;
} PACKED;

struct hba_cmd_tbl {
	uint8_t cfis[64];

	uint8_t acmd[16];

	uint8_t reserved[48];

	struct hba_prdt_entry prdt_entry[1];
} PACKED;

typedef volatile struct {
	/* 0x00 */
	paddr_t clb;
	paddr_t fb;

	/* 0x10 */
	uint32_t is;
	uint32_t ie;
	uint32_t cmd;
	uint32_t reserved;

	/* 0x20 */
	uint32_t tfd;
	uint32_t sig;
	uint32_t ssts;
	uint32_t sctl;

	/* 0x30 */
	uint32_t serr;
	uint32_t sact;
	uint32_t ci;
	uint32_t sntf;

	/* 0x40 */
	uint32_t fbs;

	uint32_t reserved1[11];
	uint32_t vendor[4];
} PACKED hbaport_t;

typedef volatile struct {
	/* 0x00 */
	uint32_t cap;
	uint32_t ghc;
	uint32_t is;
	uint32_t pi;
	uint32_t vs;

	/* 0x14 */
	uint32_t ccc_ctl;
	uint32_t ccc_ports;
	uint32_t em_loc;
	uint32_t em_ctl;
	uint32_t cap2;

	/* 0x28 */
	uint32_t bohc;

	/* 0x2C */
	uint8_t reserved[0x74];

	/* 0xA0 */
	char vendor[0x60];

	/* 0x100 */
	hbaport_t ports[32];
} PACKED hbamem_t;

typedef volatile struct {
	/* 0x00 */
	struct fis_dma_setup dfis;
	uint32_t reserved;

	/* 0x20 */
	struct fis_pio_setup pfis;
	uint32_t reserved1[3];

	/* 0x40 */
	struct fis_reg_d2h rfis;
	uint32_t reserved2;

	struct fis_set_device_bits sdbfis;

	/* 0x60 */
	uint8_t ufis[64];


	/* 0xA0 */
	uint8_t reserved3[0x60];

} PACKED hbafis_t;

void ahci_init();

#endif /* _AHCI_H_ */
