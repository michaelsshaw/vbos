/* SPDX-License-Identifier: GPL-2.0-only */
#include <kernel/common.h>
#include <kernel/acpi.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/mem.h>

#include <dev/pic.h>
#include <dev/console.h>
#include <dev/serial.h>

#include <limine/limine.h>

struct limine_hhdm_request hhdm_req = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };

uintptr_t hhdm_start;
uintptr_t data_end;
uintptr_t bss_start;
uintptr_t bss_end;

static void done(void)
{
	for (;;) {
		__asm__("hlt");
	}
}

void kmain(void)
{
	hhdm_start = hhdm_req.response->offset;
	data_end = (uintptr_t)(&__data_end);
	bss_start = (uintptr_t)(&__bss_start);
	bss_end = (uintptr_t)(&__bss_end);

	gdt_init();
	idt_init();

	serial_init();
	console_init();

	/* Initialize early kernel memory array */
	const uintptr_t one_gb = 0x40000000;
	char kmem[one_gb] ALIGN(0x1000);
	mem_early_init(kmem, one_gb);

	cli();
	pic_mask(4, 0);
	pic_init();
	printf(LOG_WARN"ATOI TEST: %d\n", atoi("101"));
	sti();
	console_resize();

	while (!console_ready())
		;

	printf("\n# ");

	done();
}
