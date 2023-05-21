/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/block.h>
#include <kernel/gpt.h>
#include <kernel/slab.h>

#include <fs/fat32.h>

#define GPT_HEADER_SIGNATURE 0x5452415020494645ull /* "EFI PART" */

void block_gpt_init(struct block_device *dev)
{
	size_t block_size = dev->block_size;

	struct gpt_header *header = kmalloc(block_size, ALLOC_KERN);
	if (!header)
		return;

	struct gpt_entry *entries = kmalloc(block_size, ALLOC_KERN);
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

	dev->partitions = kmalloc(sizeof(struct block_part) * header->entry_count, ALLOC_KERN);

	size_t used_count = 0;
	for(size_t i = 0; i < header->entry_count; i++) {
		if (entries[i].type_guid[1] == 0) 
			continue;

		used_count++;

		struct block_part *part = &dev->partitions[i];
		part->bdev = dev;
		part->lba_start = entries[i].lba_first;
		part->num_blocks = entries[i].lba_last - entries[i].lba_first + 1;

		struct fat32_fs *fs = fat_init_part(part);
		fat_print_root(fs);
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
