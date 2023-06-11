/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/gpt.h>
#include <kernel/slab.h>
#include <kernel/block.h>

#include <fs/ext2.h>

#define GPT_HEADER_SIGNATURE 0x5452415020494645ull /* "EFI PART" */

void block_gpt_init(struct block_device *dev)
{
	size_t block_size = dev->block_size;

	/* Verify that this is a GPT partition table */
	struct gpt_header *header = kmalloc(block_size, ALLOC_DMA);
	if (!header)
		return;

	struct gpt_entry *entries = kmalloc(block_size, ALLOC_DMA);
	if (!entries)
		goto free_header;

	if (block_read(dev, header, 1, 1)) {
		kprintf(LOG_ERROR "Failed to read GPT header for device %s\n", dev->name);
		goto free_entries;
	}

	if (header->signature != GPT_HEADER_SIGNATURE) {
		kprintf(LOG_ERROR "Invalid GPT header signature for device %s\n", dev->name);
		goto free_entries;
	}

	if (header->entry_size != sizeof(struct gpt_entry)) {
		kprintf(LOG_ERROR "Invalid GPT entry size for device %s\n", dev->name);
		goto free_entries;
	}

	if (block_read(dev, entries, header->lba_table, 1)) {
		kprintf(LOG_ERROR "Failed to read GPT entries for device %s\n", dev->name);
		goto free_entries;
	}

	/* Parse the GPT entries and create block devices for each partition
	 * that is found.
	 *
	 * After block device is registered, the system attempts to register a
	 * filesystem on the device if it is detected
	 */
	size_t used_count = 0;
	for (size_t i = 0; i < header->entry_count; i++) {
		struct gpt_entry *entry = &entries[i];

		if (entry->type_guid[1] == 0)
			continue;

		used_count++;

		/* Name and register the partition */
		char *buf = kzalloc(64, ALLOC_KERN);
		snprintf(buf, "%sp%d", 63, dev->name, used_count - 1);

		struct block_device *bdev = block_register(buf, &dev->ops, dev->data, dev->block_count, dev->block_size);
		if (!bdev) {
			kfree(buf);
			continue;
		}

		bdev->lba_start = entries[i].lba_first;
		bdev->block_count = entries[i].lba_last - entries[i].lba_first + 1;

		struct ext2fs *fs = ext2_init_fs(bdev);
		if (fs) {
			kprintf(LOG_INFO "Found ext2 filesystem on device %s\n", buf);
		}
	}
	dev->partition_count = used_count;

#ifdef KDEBUG
	kprintf(LOG_DEBUG "GPT header for device %s loaded\n", dev->name);
#endif

free_entries:
	kfree(entries);
free_header:
	kfree(header);
}
