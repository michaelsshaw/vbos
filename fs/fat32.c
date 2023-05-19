/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/block.h>
#include <kernel/slab.h>
#include <fs/fat32.h>

struct fat32_fs *fat_init_part(struct block_part *part)
{
	struct fat32_fs *ret = kmalloc(sizeof(struct fat32_fs), ALLOC_KERN);
	if (!ret)
		return NULL;

	ret->part = part;
	ret->dev = part->bdev;

	char *buf = kmalloc(2048, ALLOC_KERN);
	if (!buf)
		goto free_ret;

	if (block_readp(part, &ret->bpb, 0, 1)) {
		kprintf(LOG_ERROR "Failed to read block\n");
		goto free_buf;
	}

	if (ret->bpb.signature != FAT32_SIGNATURE) {
		kprintf(LOG_ERROR "Invalid FAT32 signature: %x\n", ret->bpb.signature);
		goto free_buf;
	}

	if (ret->bpb.bytes_per_sector != 512) {
		kprintf(LOG_ERROR "Invalid FAT32 bytes per sector: %d\n", ret->bpb.bytes_per_sector);
		goto free_buf;
	}

	if (block_readp(part, buf, ret->bpb.fsinfo_sector, 1)) {
		kprintf(LOG_ERROR "Failed to read fsinfo block\n");
		goto free_buf;
	}

	struct fat32_fsinfo *fsinfo = (struct fat32_fsinfo *)(buf);
	ret->fsinfo = *fsinfo;

	if (block_readp(part, buf + 512, ret->bpb.root_cluster, 1)) {
		goto free_buf;
	}

	memcpy(&ret->root, buf + 512, sizeof(struct fat32_dirent));

	kfree(buf);
	return ret;

	/* error conditions */
free_buf:
	kfree(buf);
free_ret:
	kfree(ret);
	return NULL;
}
