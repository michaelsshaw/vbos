/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/block.h>
#include <kernel/gpt.h>
#include <kernel/slab.h>

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
		printf(LOG_ERROR "Failed to read GPT header for device %s\n", dev->name);
		goto free_entries;
	}

	if (header->signature != GPT_HEADER_SIGNATURE) {
		printf(LOG_ERROR "Invalid GPT header signature for device %s\n", dev->name);
		goto free_entries;
	}

	if (header->entry_size != sizeof(struct gpt_entry)) {
		printf(LOG_ERROR "Invalid GPT entry size for device %s\n", dev->name);
		goto free_entries;
	}

	if (block_read(dev, entries, header->lba_first, 1)) {
		printf(LOG_ERROR "Failed to read GPT entries for device %s\n", dev->name);
		goto free_entries;
	}

	dev->gpt_header = header;
	dev->gpt_entries = entries;
	dev->gpt_entries_count = header->entry_count;

#ifdef KDEBUG
	printf(LOG_DEBUG "GPT header for device %s loaded\n", dev->name);
#endif
	return; /* Success */

free_entries:
	kfree(entries);
free_header:
	kfree(header);

	dev->gpt_entries = NULL;
	dev->gpt_header = NULL;
	dev->gpt_entries_count = 0;
}
