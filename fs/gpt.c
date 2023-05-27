#include <kernel/gpt.h>
#include <kernel/slab.h>
#include <kernel/block.h>

#define GPT_HEADER_SIGNATURE 0x5452415020494645ull /* "EFI PART" */

void block_gpt_init(struct block_device *dev)
{
	size_t block_size = dev->block_size;

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

	size_t used_count = 0;
	for (size_t i = 0; i < header->entry_count; i++) {
		if (entries[i].type_guid[1] == 0)
			continue;

		used_count++;

		struct block_device *bdev = kmalloc(sizeof(struct block_device), ALLOC_KERN);
		memcpy(bdev, dev, sizeof(struct block_device));

		if (!bdev)
			continue;

		bdev->name = kmalloc(64, ALLOC_KERN);
		if (!bdev->name) {
			kfree(bdev);
			continue;
		}

		snprintf(bdev->name, "%sp%d", 63, dev->name, used_count - 1);
		bdev->name[63] = '\0';

		bdev->lba_start = entries[i].lba_first;
		bdev->block_count = entries[i].lba_last - entries[i].lba_first + 1;
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
