/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _FAT32_H_
#define _FAT32_H_

#include <kernel/common.h>
#include <kernel/block.h>

#define FAT32_SIGNATURE 0x29ull

struct fat32_bpb {
	uint8_t jmp[3];
	uint64_t oem_id;
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t fat_count;
	uint16_t root_entries;
	uint16_t total_sectors;
	uint8_t media_type;
	uint16_t fat_sectors;
	uint16_t sectors_per_track;
	uint16_t heads;
	uint32_t hidden_sectors;
	uint32_t total_sectors_large;

	/* FAT32 */
	uint32_t fat_sectors_large;
	uint16_t flags;
	uint16_t version;
	uint32_t root_cluster;
	uint16_t fsinfo_sector;
	uint16_t backup_sector;
	uint8_t reserved[12];
	uint8_t drive_number;
	uint8_t reserved2;
	uint8_t signature;
	uint32_t volume_id;
	uint8_t volume_label[11];
	uint8_t system_id[8];
	uint8_t boot_code[420];
	uint16_t boot_signature;
} PACKED;

struct fat32_fsinfo {
	uint32_t signature1;
	uint8_t reserved1[480];
	uint32_t signature2;
	uint32_t free_cluster_count;
	uint32_t next_free_cluster;
	uint8_t reserved2[12];
	uint32_t signature3;
} PACKED;

struct fat32_dirent {
	uint8_t name[8];
	uint8_t ext[3];
	uint8_t attr;
	uint8_t reserved;
	uint8_t ctime_ms;
	uint16_t ctime_time;
	uint16_t ctime_date;
	uint16_t atime_date;
	uint16_t cluster_high;
	uint16_t mtime_time;
	uint16_t mtime_date;
	uint16_t cluster_low;
	uint32_t size;
} PACKED;

struct fat32_fs {
	struct block_device *dev;
	struct block_part *part;

	struct fat32_bpb bpb;
	struct fat32_fsinfo fsinfo;
	struct fat32_dirent root;
};

struct fat32_fs *fat_init_part(struct block_part *part);
void fat_print_root(struct fat32_fs *fs);

#endif /* _FAT32_H_ */
