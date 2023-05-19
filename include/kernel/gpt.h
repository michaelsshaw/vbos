/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _GPT_H_
#define _GPT_H_

#include <kernel/common.h>

struct block_device;

/* GPT LBA 0 */
struct gpt_mbr_prot {
	uint8_t boot[1];
	uint8_t chs_start[3];
	uint8_t type[1];
	uint8_t chs_end[3];
	uint8_t lba_start[4];
	uint8_t lba_size[4];
} PACKED;

/* GPT LBA 1 
 * checksums use CRC32
 */
struct gpt_header {
	uint64_t signature;
	uint8_t revision[4];
	uint8_t header_size[4];
	uint8_t checksum[4];
	uint8_t reserved[4];
	uint64_t lba_this;
	uint64_t lba_alt;
	uint64_t lba_first;
	uint64_t lba_last;
	uint8_t guid[16];
	uint64_t lba_table;
	uint32_t entry_count;
	uint32_t entry_size;
	uint32_t checksum_table;
} PACKED;

/* GPT LBA 2 */
struct gpt_entry {
	uint64_t type_guid[2];
	uint64_t unique_guid[2];
	uint64_t lba_first;
	uint64_t lba_last;
	uint64_t flags;
	uint8_t name[72];
} PACKED;

void block_gpt_init(struct block_device *dev);

#endif /* _GPT_H_ */

