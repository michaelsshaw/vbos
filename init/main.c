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

#include <limine/limine.h>

#define KSTACK_SIZE 0x4000

void stack_init(uintptr_t rsp, uintptr_t rbp);

struct limine_hhdm_request hhdm_req = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };

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
	iowait();
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

	console_init();
	acpi_init();

	_pic_init();

	void *kstack = buddy_alloc(KSTACK_SIZE);
	uintptr_t ptr = (uintptr_t)kstack + KSTACK_SIZE - 8;
	stack_init(ptr, ptr);
}

void kmain_post_stack_init()
{
	pci_init();

	printf("\n\n# ");
	yield();
}
