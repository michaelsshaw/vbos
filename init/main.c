/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mem.h>
#include <kernel/slab.h>
#include <kernel/pio.h>

#include <dev/pic.h>
#include <dev/console.h>
#include <dev/serial.h>
#include <dev/pci.h>
#include <dev/ahci.h>

#define LIMINE_INTERNAL_MODULE_REQUIRED (1 << 0)
#include <limine/limine.h>

#define KSTACK_SIZE 0x4000

void stack_init(uintptr_t rsp, uintptr_t rbp);

struct limine_hhdm_request hhdm_req = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };
struct limine_kernel_file_request module_req = { .id = LIMINE_KERNEL_FILE_REQUEST, .revision = 0 };

static char *kcmdline;
static char *kpath;
static struct limine_uuid kroot_disk;
static struct limine_uuid kroot_part;

uintptr_t hhdm_start;
uintptr_t data_end;
uintptr_t bss_start;
uintptr_t bss_end;

void yield()
{
	for (;;) {
		__asm__("hlt");
	}
}

static inline void _pic_init()
{
	cli();
	pic_mask(4, 0);
	pic_init();
	sti();
	console_resize();

	while (!console_ready())
		;
}

void kmain(void)
{
	hhdm_start = hhdm_req.response->offset;
	data_end = (uintptr_t)(&__data_end);
	bss_start = (uintptr_t)(&__bss_start);
	bss_end = (uintptr_t)(&__bss_end);

	serial_init();
	gdt_init();
	idt_init();

	/* Initialize early kernel memory array */
	const uintptr_t one_gb = 0x40000000;
	char kmem[one_gb] ALIGN(0x1000);
	mem_early_init(kmem, one_gb);
	kmalloc_init();

	struct limine_kernel_file_response *resp = module_req.response;

	struct limine_file *kfile = (struct limine_file *)((paddr_t)resp->kernel_file | hhdm_start);

	kcmdline = kzalloc(strlen(kfile->cmdline) + 1, ALLOC_KERN);
	strcpy(kcmdline, kfile->cmdline);

	kpath = kzalloc(strlen(kfile->path) + 1, ALLOC_KERN);
	strcpy(kpath, kfile->path);

	kroot_disk = kfile->gpt_disk_uuid;
	kroot_part = kfile->gpt_part_uuid;

#ifdef KDEBUG
	printf(LOG_DEBUG "Kernel file: path='%s' cmdline='%s'\n", kpath, kcmdline);
	printf(LOG_DEBUG "Root partition: disk=%X-%X part=%X-%X\n", kroot_disk.a, kroot_disk.b, kroot_part.a, kroot_part.b);
#endif /* KDEBUG */

	console_init();
	acpi_init();

	pci_init();

	void *kstack = buddy_alloc(KSTACK_SIZE);
	uintptr_t ptr = (uintptr_t)kstack + KSTACK_SIZE - 8;
	stack_init(ptr, ptr);
}

void kmain_post_stack_init()
{
	ahci_init();

	_pic_init();

	printf("\n\n# ");
	yield();
}
